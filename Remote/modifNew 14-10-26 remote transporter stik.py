import pygame
import serial
import time

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

# === INISIALISASI JOYSTICK ===
pygame.init()
pygame.joystick.init()

if pygame.joystick.get_count() == 0:
    raise Exception("🎮 Tidak ada joystick terdeteksi!")

j = pygame.joystick.Joystick(0)
j.init()
print(f"🎮 Joystick: {j.get_name()}")
print(f"🧩 Tombol={j.get_numbuttons()}, Axis={j.get_numaxes()}, Hat={j.get_numhats()}")

# === KIRIM PILOT KE ROBOT (robot masuk ke gigi NORMAL milik pilot ini) ===
bt.write(f"PILOT,{PILOT_AKTIF}\n".encode())
print(f"📡 Preset pilot {PILOT_AKTIF.capitalize()} dikirim ke robot")
time.sleep(0.2)

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
# STATE
# ============================================================
button_states        = [0] * j.get_numbuttons()
trigger_states       = [0, 0]
trigger_axis_indices = [4, 5]
TRIGGER_THRESHOLD    = 0.5
is_slow_mode         = False
last_hat             = (0, 0)
dpad_active          = False

last_btn_time        = [0.0] * j.get_numbuttons()
last_trig_time       = [0.0, 0.0]
last_hat_time        = 0.0
last_hat_send        = 0.0  # Untuk tracking DPAD_REPEAT_INTERVAL
last_analog_time     = 0.0
last_lifter_send     = 0.0
last_lifter_cmd_time = 0.0
last_lifter_val      = 0.0


def safe_send(msg):
    try:
        bt.write(msg.encode())
    except serial.SerialException as e:
        print(f"⚠️ Serial error: {e}")


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


try:
    while True:
        pygame.event.pump()
        now = time.time()

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

        time.sleep(0.02)  # ~50 Hz

except KeyboardInterrupt:
    print("\n🛑 Stop — kirim perintah berhenti...")
    safe_send("HAT,0,0\n")
    safe_send("AXIS,0.000,0.000,0.000\n")
    safe_send("LIFTER_SPD,0.000\n")
    time.sleep(0.2)
    bt.close()
    pygame.quit()