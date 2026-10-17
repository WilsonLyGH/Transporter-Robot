import pygame
import serial
import time
import msvcrt  # baca keyboard non-blocking (Windows) untuk ganti pilot

# === KONFIGURASI ===
BT_PORT = 'COM3'  # KUYA
# BT_PORT = 'COM16'  # RESP
BT_BAUD = 9600

# === DAFTAR PILOT ===
# Tiap pilot punya preset NORMAL & MAX sendiri (disimpan di EEPROM robot)
PILOTS = ["ZAKY", "DAFFI"]


def pilih_pilot():
    print("\n==================================")
    print("   PILIH PILOT")
    for idx, nama in enumerate(PILOTS, start=1):
        print(f"   [{idx}] {nama.capitalize()}")
    print("==================================")
    while True:
        pilihan = input("Masukkan nomor / nama pilot: ").strip().upper()
        if pilihan.isdigit() and 1 <= int(pilihan) <= len(PILOTS):
            return PILOTS[int(pilihan) - 1]
        if pilihan in PILOTS:
            return pilihan
        print("❌ Pilihan tidak valid, coba lagi.")


PILOT_AKTIF = pilih_pilot()
print(f"🧑‍✈️ Pilot dipilih: {PILOT_AKTIF.capitalize()}")

# === INISIALISASI SERIAL (HC-12 / BLUETOOTH) ===
bt = serial.Serial(BT_PORT, BT_BAUD, timeout=0.1)
time.sleep(2)
print(f"✅ Terhubung ke modul komunikasi di {BT_PORT}")


def safe_send(msg):
    try:
        bt.write(msg.encode())
    except serial.SerialException as e:
        print(f"⚠️ Serial error: {e}")


# === KIRIM PILOT KE ROBOT (robot masuk ke gigi NORMAL milik pilot ini) ===
def kirim_pilot(nama):
    global PILOT_AKTIF
    PILOT_AKTIF = nama
    safe_send(f"PILOT,{nama}\n")
    print(f"📡 Preset pilot {nama.capitalize()} dikirim ke robot (gigi NORMAL)")


kirim_pilot(PILOT_AKTIF)
time.sleep(0.2)

# === GANTI PILOT & EDIT PRESET TANPA RESTART ===
#   1 / Z  → Zaky
#   2 / D  → Daffi
#   P      → ganti ke pilot berikutnya
#   L      → List (lihat semua preset yang tersimpan di robot)
#   E      → Edit nilai preset (kDepan, bMaju, bStrafe, omega)
PILOT_KEYS = {"1": "ZAKY", "Z": "ZAKY", "2": "DAFFI", "D": "DAFFI"}
print("⌨️  Keyboard: [1/Z] Zaky  [2/D] Daffi  [P] Next Pilot  [L] Lihat Preset  [E] Edit Preset")


def tampilkan_semua_preset():
    """Minta robot mengirim data semua preset lalu tampilkan ke terminal."""
    print("\n⏳ Mengambil data preset dari robot...")
    safe_send("GET\n")
    data_presets = {}
    timeout = time.time() + 2.0
    active_info = ""

    while time.time() < timeout:
        if bt.in_waiting > 0:
            line = bt.readline().decode('utf-8', errors='ignore').strip()
            if line.startswith("PRESET_DATA,"):
                # Format: PRESET_DATA,PILOT,MODE,kDepan,bMaju,bStrafe,omega
                parts = line.split(',')
                if len(parts) >= 7:
                    p_nama = parts[1]
                    m_idx  = "NORMAL" if parts[2] == "0" else "MAX"
                    k_val  = parts[3]
                    bm_val = parts[4]
                    bs_val = parts[5]
                    om_val = parts[6]
                    if p_nama not in data_presets:
                        data_presets[p_nama] = {}
                    data_presets[p_nama][m_idx] = (k_val, bm_val, bs_val, om_val)
            elif line.startswith("PRESET_END,"):
                parts = line.split(',')
                if len(parts) >= 3:
                    m_str = "NORMAL" if parts[2] == "0" else "MAX"
                    active_info = f"Aktif saat ini: {parts[1]} ({m_str})"
                break
        time.sleep(0.01)

    if not data_presets:
        print("⚠️ Tidak ada respon dari robot (pastikan Arduino terhubung).")
        return data_presets

    print("\n" + "=" * 55)
    print(" 📋 DATA PRESET DI ROBOT (EEPROM)")
    print("=" * 55)
    for p_nama in ["ZAKY", "DAFFI"]:
        if p_nama in data_presets:
            print(f" 🧑‍✈️ [{p_nama}]")
            for m_str in ["NORMAL", "MAX"]:
                if m_str in data_presets[p_nama]:
                    k, bm, bs, om = data_presets[p_nama][m_str]
                    print(f"    - {m_str:<6} : kDepan={k:<5} | bMaju={bm:<4} | bStrafe={bs:<4} | Omega={om:<4}")
    if active_info:
        print("-" * 55)
        print(f" ℹ️  {active_info}")
    print("=" * 55 + "\n")
    return data_presets


def menu_edit_preset():
    """Mode interaktif untuk mengedit nilai preset secara langsung."""
    # Berhentikan robot terlebih dahulu demi keamanan
    safe_send("AXIS,0.000,0.000,0.000\n")
    safe_send("LIFTER_SPD,0.000\n")

    print("\n" + "#" * 55)
    print(" 🛠️  MENU EDIT PRESET (LIVE TUNING MANUAL)")
    print(" (Robot dihentikan sementara selama mode edit)")
    print("#" * 55)

    # Ambil data saat ini agar user tahu nilai lama
    data_presets = tampilkan_semua_preset()

    # 1. Pilih Pilot
    print("Pilih Pilot yang ingin diedit:")
    print(f" [1] Zaky  {' (Aktif)' if PILOT_AKTIF == 'ZAKY' else ''}")
    print(f" [2] Daffi {' (Aktif)' if PILOT_AKTIF == 'DAFFI' else ''}")
    p_in = input(f"Pilihan Pilot [Enter = {PILOT_AKTIF}]: ").strip().upper()
    if p_in in ["1", "Z", "ZAKY"]:
        target_pilot = "ZAKY"
    elif p_in in ["2", "D", "DAFFI"]:
        target_pilot = "DAFFI"
    else:
        target_pilot = PILOT_AKTIF

    # 2. Pilih Mode Gigi
    print(f"\nPilih Mode Gigi untuk {target_pilot}:")
    print(" [0] NORMAL (Segitiga)")
    print(" [1] MAX    (Tombol X)")
    m_in = input("Pilihan Gigi [0/1, Enter = 0 / NORMAL]: ").strip()
    if m_in in ["1", "MAX", "M"]:
        target_mode_idx = "1"
        mode_label = "MAX"
    else:
        target_mode_idx = "0"
        mode_label = "NORMAL"

    # Ambil nilai default/lama sebagai acuan
    default_vals = ("0.18", "2.8", "1.6", "1.2")
    if target_pilot in data_presets and mode_label in data_presets[target_pilot]:
        default_vals = data_presets[target_pilot][mode_label]

    print(f"\n✏️ Mengedit [{target_pilot} - {mode_label}]:")
    print("*(Tekan Enter langsung jika tidak ingin mengubah nilai)")

    def prompt_float(nama, current_val):
        while True:
            val_in = input(f" - {nama:<8} [saat ini: {current_val}]: ").strip()
            if val_in == "":
                return current_val
            try:
                f = float(val_in)
                return str(f)
            except ValueError:
                print("   ❌ Masukkan angka desimal yang valid (misal: 0.15 atau 2.5)!")

    k_new  = prompt_float("kDepan",  default_vals[0])
    bm_new = prompt_float("bMaju",   default_vals[1])
    bs_new = prompt_float("bStrafe", default_vals[2])
    om_new = prompt_float("omega",   default_vals[3])

    cmd_set = f"SET,{target_pilot},{target_mode_idx},{k_new},{bm_new},{bs_new},{om_new}\n"
    print(f"\n📡 Mengirim ke robot: {cmd_set.strip()}")
    safe_send(cmd_set)

    # Tunggu feedback dari Arduino
    time.sleep(0.3)
    while bt.in_waiting > 0:
        balasan = bt.readline().decode('utf-8', errors='ignore').strip()
        if balasan:
            print(f"🤖 Robot: {balasan}")

    print("\n✅ Nilai preset berhasil diperbarui & disimpan di EEPROM!")
    print("🎮 Kembali ke kontrol joystick...\n")
    pygame.event.clear()


def cek_keyboard_pilot():
    while msvcrt.kbhit():
        key = msvcrt.getwch().upper()
        if key in PILOT_KEYS:
            target = PILOT_KEYS[key]
            if target != PILOT_AKTIF:
                print(f"\n🔄 Ganti pilot: {PILOT_AKTIF.capitalize()} → {target.capitalize()}")
                kirim_pilot(target)
            else:
                print(f"ℹ️  Pilot {target.capitalize()} sudah aktif")
        elif key == "P":
            target = PILOTS[(PILOTS.index(PILOT_AKTIF) + 1) % len(PILOTS)]
            print(f"\n🔄 Ganti pilot: {PILOT_AKTIF.capitalize()} → {target.capitalize()}")
            kirim_pilot(target)
        elif key == "L":
            tampilkan_semua_preset()
        elif key == "E":
            menu_edit_preset()
        else:
            continue

# ============================================================
# MAPPING TOMBOL (SPEED & KALIBRASI KDEPAN)
# ============================================================
PRESET_MAP = {
    0: ("CUSTOM_CMD", "SPEED,MAX\n",       "X/Cross     (Mode SPEED MAKSIMUM)"),
    1: ("CUSTOM_CMD", "KDEPAN,DOWN\n",     "O/Circle    (kDepan Turun -0.02)"),
    2: ("CUSTOM_CMD", "KDEPAN,UP\n",       "Kotak       (kDepan Naik +0.02)"),
    3: ("CUSTOM_CMD", "SPEED,NORMAL\n",    "Segitiga    (Mode SPEED NORMAL)"),
    6: ("CUSTOM_CMD", "SPEED_ADJ,UP\n",    "Options     (Kecepatan Maju/Strafe Naik +0.1)"),
    7: ("CUSTOM_CMD", "SPEED_ADJ,DOWN\n",  "Share       (Kecepatan Maju/Strafe Turun -0.1)"),
    8: ("CUSTOM_CMD", "OMEGA_ADJ,DOWN\n",  "L3          (Kecepatan Putar Turun -0.1)"),
    9: ("CUSTOM_CMD", "OMEGA_ADJ,UP\n",    "R3          (Kecepatan Putar Naik +0.1)"),
}
LIFTER_BTN_INDICES = set(PRESET_MAP.keys())

# ============================================================
# DEBOUNCE & TIMING
# ============================================================
DEBOUNCE_BTN    = 0.25
DEBOUNCE_LIFTER = 0.20
DEBOUNCE_HAT    = 0.02
LIFTER_COOLDOWN = 0.20

ANALOG_DEADZONE = 0.01
ANALOG_INTERVAL = 0.05  # 20 Hz

DPAD_REPEAT_INTERVAL = 0.10

# ============================================================
# LIFTER DINAMIS (analog kanan Y)
# ============================================================
LIFTER_DEADZONE      = 0.10
LIFTER_SEND_INTERVAL = 0.05
MAX_DYNAMIC_SPEED    = 900.0

# ============================================================
# STATE JOYSTICK & AUTO-RECONNECT
# ============================================================
j = None
joystick_connected = False
button_states = []
trigger_states = [0, 0]
trigger_axis_indices = [4, 5]
TRIGGER_THRESHOLD = 0.5
is_slow_mode = False
last_hat = (0, 0)
dpad_active = False

last_btn_time = []
last_trig_time = [0.0, 0.0]
last_hat_time = 0.0
last_hat_send = 0.0  # Untuk tracking DPAD_REPEAT_INTERVAL
last_analog_time = 0.0
last_lifter_send = 0.0
last_lifter_cmd_time = 0.0
last_lifter_val = 0.0
last_reconnect_check = 0.0


def init_joystick():
    """Inisialisasi ulang joystick (mendukung hot-plug / auto-reconnect)."""
    global j, joystick_connected, button_states, last_btn_time
    global trigger_states, last_trig_time, last_hat, dpad_active, last_lifter_val

    try:
        pygame.joystick.quit()
        pygame.joystick.init()

        if pygame.joystick.get_count() > 0:
            j = pygame.joystick.Joystick(0)
            j.init()
            num_btns = j.get_numbuttons()
            button_states = [0] * num_btns
            last_btn_time = [0.0] * num_btns
            trigger_states = [0, 0]
            last_trig_time = [0.0, 0.0]
            last_hat = (0, 0)
            dpad_active = False
            last_lifter_val = 0.0
            joystick_connected = True
            print(f"🎮 Joystick: {j.get_name()}")
            print(f"🧩 Tombol={num_btns}, Axis={j.get_numaxes()}, Hat={j.get_numhats()}")
            return True
    except Exception as e:
        pass

    j = None
    joystick_connected = False
    return False


def handle_disconnect():
    """Menangani ketika stik terputus / idle sleep demi keamanan robot."""
    global joystick_connected, j, dpad_active, last_hat, last_lifter_val

    if joystick_connected:
        joystick_connected = False
        j = None
        dpad_active = False
        last_hat = (0, 0)
        last_lifter_val = 0.0

        # Kirim sinyal henti ke robot segera
        safe_send("AXIS,0.000,0.000,0.000\n")
        safe_send("LIFTER_SPD,0.000\n")
        safe_send("HAT,0,0\n")

        print("\n" + "=" * 55)
        print("⚠️  [STIK TERPUTUS / IDLE SLEEP]")
        print("🛑 Robot otomatis dihentikan demi keamanan!")
        print("⏳ Menunggu stik tersambung kembali... (tekan tombol di stik)")
        print("=" * 55 + "\n")


def apply_deadzone(val, zone):
    """Deadzone dengan re-normalisasi agar tidak ada lompatan di tepi zone."""
    if abs(val) < zone:
        return 0.0
    sign = 1.0 if val > 0 else -1.0
    return round(sign * (abs(val) - zone) / (1.0 - zone), 3)


def format_lifter(raw_axis):
    """Konversi raw axis kanan Y → nilai lifter (-1 s/d +1)."""
    val = -raw_axis  # balik: push up → positif
    if abs(val) < LIFTER_DEADZONE:
        return 0.0
    sign = 1.0 if val > 0 else -1.0
    return round(sign * (abs(val) - LIFTER_DEADZONE) / (1.0 - LIFTER_DEADZONE), 3)


# === INISIALISASI PYGAME & STIK AWAL ===
pygame.init()
pygame.joystick.init()
print("🔍 Memeriksa stik controller...")
if not init_joystick():
    print("⚠️  Stik belum terdeteksi. Silakan nyalakan stik...")
    print("⏳ Menunggu stik tersambung (program tetap berjalan)...")


try:
    while True:
        now = time.time()

        # ============================================================
        # AUTO-RECONNECT: DETEKSI EVENT STIK (HOT-PLUG)
        # ============================================================
        for event in pygame.event.get():
            if event.type == pygame.JOYDEVICEREMOVED:
                handle_disconnect()
            elif event.type == pygame.JOYDEVICEADDED:
                time.sleep(0.15)
                if init_joystick():
                    print("\n" + "=" * 55)
                    print("✅ [RECONNECTED] Stik terhubung kembali!")
                    print("🎮 Siap dimainkan kembali!")
                    print("=" * 55 + "\n")

        # Fallback reconnect jika event tidak tertembak (cek berkala tiap 1 detik)
        if not joystick_connected:
            if now - last_reconnect_check >= 1.0:
                last_reconnect_check = now
                if pygame.joystick.get_count() > 0:
                    if init_joystick():
                        print("\n" + "=" * 55)
                        print("✅ [RECONNECTED] Stik terhubung kembali!")
                        print("🎮 Siap dimainkan kembali!")
                        print("=" * 55 + "\n")

        # --- Cek tombol keyboard untuk ganti pilot ---
        cek_keyboard_pilot()

        # ============================================================
        # BACA PESAN DARI ARDUINO & TAMPILKAN KE TERMINAL PYTHON
        # ============================================================
        while bt.in_waiting > 0:
            try:
                balasan_arduino = bt.readline().decode('utf-8', errors='ignore').strip()
                if balasan_arduino:
                    print(balasan_arduino)
            except Exception as e:
                pass
        # ============================================================

        # ============================================================
        # PROSES INPUT STIK (HANYA JIKA TERKONEKSI)
        # ============================================================
        if joystick_connected and j is not None:
            try:
                # ============================================================
                # ANALOG KIRI → translasi robot (20 Hz)
                # Skip kirim AXIS saat D-pad aktif agar Vx/Vy tidak tertimpa.
                # ============================================================
                if now - last_analog_time >= ANALOG_INTERVAL:
                    if not dpad_active:
                        x_left = apply_deadzone(j.get_axis(0), ANALOG_DEADZONE)
                        y_left = apply_deadzone(j.get_axis(1), ANALOG_DEADZONE)
                        safe_send(f"AXIS,{x_left:.3f},{y_left:.3f},0.000\n")
                    last_analog_time = now

                # ============================================================
                # ANALOG KANAN Y → Lifter DINAMIS
                # ============================================================
                if j.get_numaxes() > 3:
                    y_right    = j.get_axis(3)
                    lifter_val = format_lifter(y_right)

                    value_changed = abs(lifter_val - last_lifter_val) > 0.02
                    is_active     = abs(lifter_val) > 0.0
                    interval_ok   = (now - last_lifter_send) >= LIFTER_SEND_INTERVAL

                    if value_changed or (is_active and interval_ok):
                        safe_send(f"LIFTER_SPD,{lifter_val:.3f}\n")
                        last_lifter_send = now
                        last_lifter_val  = lifter_val
                        if value_changed:
                            if lifter_val == 0.0:
                                print("Lifter: STOP")
                            else:
                                print(f"Lifter: {'NAIK' if lifter_val > 0 else 'TURUN'} spd={lifter_val:.3f}")

                # ============================================================
                # TOMBOL DIGITAL
                # ============================================================
                for i in range(j.get_numbuttons()):
                    pressed = j.get_button(i)
                    if pressed != button_states[i]:
                        if pressed == 1:
                            if i in LIFTER_BTN_INDICES:
                                if now - last_lifter_cmd_time < LIFTER_COOLDOWN:
                                    print(f"⛔ [{i}] lifter cooldown")
                                    button_states[i] = pressed
                                    continue
                                if now - last_btn_time[i] >= DEBOUNCE_LIFTER:
                                    button_states[i]     = pressed
                                    last_btn_time[i]     = now
                                    last_lifter_cmd_time = now
                                    cmd_type, height, label = PRESET_MAP[i]
                                    if cmd_type == "HOMING":
                                        safe_send("HOMING\n")
                                    elif cmd_type == "PRESET":
                                        safe_send(f"PRESET,{height:.1f}\n")
                                    elif cmd_type == "CUSTOM_CMD":
                                        safe_send(height) # height di sini berisi perintah string seperti "SPEED,MAX\n"
                                    elif cmd_type == "DISABLED":
                                        pass
                                        
                                    print(f"🎯 {label}")
                                else:
                                    print(f"⛔ [{i}] debounce")
                            else:
                                if now - last_btn_time[i] >= DEBOUNCE_BTN:
                                    button_states[i] = pressed
                                    last_btn_time[i] = now
                                    safe_send(f"BUTTON,{i},{pressed}\n")
                                    print(f"BUTTON,{i},1")
                                else:
                                    print(f"⛔ [{i}] debounce")
                        else:
                            button_states[i] = pressed
                            if i not in LIFTER_BTN_INDICES:
                                safe_send(f"BUTTON,{i},{pressed}\n")

                # ============================================================
                # TRIGGER L2 / R2
                # ============================================================
                for t, ax in enumerate(trigger_axis_indices):
                    if ax < j.get_numaxes():
                        pressed = 1 if j.get_axis(ax) > TRIGGER_THRESHOLD else 0
                        if pressed != trigger_states[t]:
                            if pressed == 1:
                                if now - last_trig_time[t] >= DEBOUNCE_BTN:
                                    trigger_states[t] = pressed
                                    last_trig_time[t] = now
                                    safe_send(f"BUTTON,TRIG{t},{pressed}\n")

                                    # Keterangan Terminal untuk L2 dan R2
                                    if t == 0:  # Jika L2 (TRIG0) ditekan
                                        print("🎯 [L2] Gripper 1 Toggle")
                                    elif t == 1:  # Jika R2 (TRIG1) ditekan
                                        print("🎯 [R2] Gripper 2 Toggle")
                                else:
                                    print(f"⛔ Trigger[{t}] debounce")
                            else:
                                trigger_states[t] = pressed
                                safe_send(f"BUTTON,TRIG{t},{pressed}\n")

                # ============================================================
                # D-PAD (HAT)
                # ============================================================
                if j.get_numhats() > 0:
                    hat = j.get_hat(0)

                    # Kirim saat state berubah (tekan / lepas)
                    if hat != last_hat and now - last_hat_time >= DEBOUNCE_HAT:
                        last_hat      = hat
                        last_hat_time = now
                        last_hat_send = now  # Reset timer repeat
                        dpad_active   = (hat != (0, 0))
                        safe_send(f"HAT,{hat[0]},{hat[1]}\n")
                        print(f"HAT: {hat[0]},{hat[1]}")

                    # Kirim ulang selama D-pad masih ditahan (repeat)
                    elif dpad_active and (now - last_hat_send >= DPAD_REPEAT_INTERVAL):
                        last_hat_send = now
                        safe_send(f"HAT,{last_hat[0]},{last_hat[1]}\n")

            except pygame.error:
                # Terputus di tengah operasi polling
                handle_disconnect()

        time.sleep(0.02)  # ~50 Hz

except KeyboardInterrupt:
    print("\n🛑 Stop — kirim perintah berhenti...")
    safe_send("HAT,0,0\n")
    safe_send("AXIS,0.000,0.000,0.000\n")
    safe_send("LIFTER_SPD,0.000\n")
    time.sleep(0.2)
    bt.close()
    pygame.quit()