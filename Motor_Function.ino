void readEncoder1() {
  if (digitalRead(ENC1A) == digitalRead(ENC1B)) {
    encoderPosition[1]--;
  } else {
    encoderPosition[1]++;
  }
}

void readEncoder2() {
  if (digitalRead(ENC2A) == digitalRead(ENC2B)) {
    encoderPosition[2]--;
  } else {
    encoderPosition[2]++;
  }
}

void readEncoder3() {
  if (digitalRead(ENC3A) == digitalRead(ENC3B)) {
    encoderPosition[3]--;
  } else {
    encoderPosition[3]++;
  }
}

void encoderToVelocity() {
  unsigned long currentMillis = millis();
  float deltaT = (currentMillis - lastMillis); 

  if (deltaT >= 50) { 
    float timeInSeconds = deltaT / 1000.0;
    for (int i = 1; i < 4; i++) {
      pulsePerSecond[i] = (encoderPosition[i] - lastPulse[i]); 
      lastPulse[i] = encoderPosition[i];
      velocityEncoder[i] = (float)(pulsePerSecond[i] * 60) / (PPR * (deltaT / 1000.0));  
    }
    lastMillis = currentMillis;
  }
}

void inversKinematic() {
  static float smoothVx = 0.0;
  static float smoothVy = 0.0;
  static unsigned long lastRampTime = 0;

  if (millis() - lastRampTime >= 10) {
    float accelStep = 0.10; // Ramping akselerasi

    if (Vx == 0) smoothVx = 0;
    else if (abs(Vx - smoothVx) > accelStep) smoothVx += (Vx > smoothVx) ? accelStep : -accelStep;
    else smoothVx = Vx;

    if (Vy == 0) smoothVy = 0;
    else if (abs(Vy - smoothVy) > accelStep) smoothVy += (Vy > smoothVy) ? accelStep : -accelStep;
    else smoothVy = Vy;

    lastRampTime = millis();
  }

  float factor   = 60.0 / (WHEEL_CIRCUMFERENCE);
  float targetVx = smoothVx * boostStrafe;
  float targetVy = smoothVy * boostMaju;

  Vmot[1] = ((targetVx + (L * Omega)) * factor);
  Vmot[2] = (((0.866 * targetVy) - (kDepan * targetVx) + (L * Omega)) * factor);
  Vmot[3] = ((-(0.866 * targetVy) - (kDepan * targetVx) + (L * Omega)) * factor);
}
// ============================================================
// readSerial — membaca semua perintah dari Python via Serial3
//
// Perintah yang masuk:
//   AXIS,xL,yL,0.00     → translasi robot (analog kiri)
//   LIFTER_SPD,<val>    → kontrol lifter dinamis (-1.0 s/d +1.0)
//   PRESET,<height>     → preset ketinggian lifter (cm)
//   HOMING              → mulai proses homing lifter B
//   BUTTON,<i>,<state>  → tombol digital (rotasi, gripper, dll)
//   BUTTON,TRIG<t>,<s>  → trigger L2/R2
//   HAT,<x>,<y>         → D-Pad
// ============================================================
// Fungsi untuk memuat nilai kalibrasi dari EEPROM saat robot pertama kali nyala
// --- FUNGSI MEMBACA MEMORI SAAT ROBOT NYALA ---
void loadCalibrationFromEEPROM() {
  for (int p = 0; p < 2; p++) {
    for (int m = 0; m < 2; m++) {
      float tK, tBM, tBS, tO;
      EEPROM.get(addrKDepan(p, m),  tK);
      EEPROM.get(addrBMaju(p, m),   tBM);
      EEPROM.get(addrBStrafe(p, m), tBS);
      EEPROM.get(addrOmega(p, m),   tO);

      // Validasi: Cegah data error jika EEPROM masih kosong (NaN gagal semua perbandingan)
      if (tK  >= 0.0 && tK  <= 1.0)  presets[p][m].kDepan  = tK;
      if (tBM >= 0.0 && tBM <= 10.0) presets[p][m].bMaju   = tBM;
      if (tBS >= 0.0 && tBS <= 10.0) presets[p][m].bStrafe = tBS;
      if (tO  >= 0.0 && tO  <= 10.0) presets[p][m].omega   = tO;
    }
  }

  // Pilot terakhir yang dipakai
  byte savedPilot = EEPROM.read(ADDR_ACTIVE_PILOT);
  currentPilot = (savedPilot == PILOT_DAFFI) ? PILOT_DAFFI : PILOT_ZAKY;

  // Setel nilai aktif saat awal nyala (Masuk ke Gigi Normal)
  applyPreset(currentPilot, 0, false);
  
  Serial.print("✅ Memori Kalibrasi Berhasil Dimuat! Pilot: ");
  Serial.println(PILOT_NAME[currentPilot]);
}

// --- FUNGSI MENYIMPAN MEMORI (pilot + gigi yang sedang aktif) ---
void saveCalibrationToEEPROM() {
  int p = currentPilot;
  int m = currentSpeedMode;

  presets[p][m].kDepan  = kDepan;
  presets[p][m].bMaju   = boostMaju;
  presets[p][m].bStrafe = boostStrafe;
  presets[p][m].omega   = omegaforces;

  EEPROM.put(addrKDepan(p, m),  kDepan);
  EEPROM.put(addrBMaju(p, m),   boostMaju);
  EEPROM.put(addrBStrafe(p, m), boostStrafe);
  EEPROM.put(addrOmega(p, m),   omegaforces);

  Serial3.print("Tersimpan ke EEPROM (");
  Serial3.print(PILOT_NAME[p]);
  Serial3.println(m == 0 ? " - GIGI NORMAL)" : " - GIGI MAKSIMUM)");
}

// --- TERAPKAN PRESET pilot + gigi ke variabel aktif ---
void applyPreset(int pilot, int mode, bool verbose) {
  currentPilot     = pilot;
  currentSpeedMode = mode;

  kDepan      = presets[pilot][mode].kDepan;
  boostMaju   = presets[pilot][mode].bMaju;
  boostStrafe = presets[pilot][mode].bStrafe;
  omegaforces = presets[pilot][mode].omega;

  if (!verbose) return;
  Serial3.println("\n==================================");
  Serial3.print("PILOT: "); Serial3.println(PILOT_NAME[pilot]);
  Serial3.println(mode == 1 ? "GIGI : MAKSIMUM (Tombol X)" : "GIGI : NORMAL (Tombol Segitiga)");
  Serial3.print("   Speed Maju   : "); Serial3.println(boostMaju, 1);
  Serial3.print("   Speed Strafe : "); Serial3.println(boostStrafe, 1);
  Serial3.print("   Speed Putar  : "); Serial3.println(omegaforces, 1);
  Serial3.print("   kDepan       : "); Serial3.println(kDepan, 3);
  Serial3.println("==================================\n");
}
void readSerial() {
  // 1. Membaca huruf satu per satu tanpa menghentikan program (Non-Blocking)
  while (Serial3.available()) {
    char inChar = (char)Serial3.read();
    inputString += inChar;
    if (inChar == '\n') {
      stringComplete = true;
    }
  }

  // 2. Jika 1 kalimat utuh sudah masuk penampungan, baru kita proses
  if (stringComplete) {
    lastSignalTime = millis(); // Reset timer Failsafe
    
    String dt = inputString;
    dt.trim();
    
    inputString = "";       // Kosongkan penampungan untuk pesan berikutnya
    stringComplete = false; // Reset status

    if (dt.length() == 0) return;

    // --- AXIS: translasi analog kiri ---
    if (dt.startsWith("AXIS,")) {
      int c1 = dt.indexOf(',');
      int c2 = dt.indexOf(',', c1 + 1);
      int c3 = dt.indexOf(',', c2 + 1);

      float xL = dt.substring(c1 + 1, c2).toFloat();
      float yL = dt.substring(c2 + 1, c3).toFloat();
      float xR = dt.substring(c3 + 1).toFloat();

      parseData("X" + String(xL, 2) + "Y" + String(yL, 2) + "R" + String(xR, 2));
    }

    // --- LIFTER_SPD: kontrol dinamis analog kanan Y ---
    // Nilai float -1.0 s/d +1.0 dikirim 20 Hz selama stick aktif.
    // Switch ke DYNAMIC mode; updateLifterDynamic() di loop() yang eksekusi.
    else if (dt.startsWith("LIFTER_SPD,")) {
      float spd = dt.substring(11).toFloat();
      lifterSpdValue = constrain(spd, -1.0, 1.0);
      lifterMode = LIFTER_DYNAMIC;

      if (lifterSpdValue != 0.0) {
        String arah = (lifterSpdValue > 0) ? "NAIK" : "TURUN";
        Serial.print("Lifter dinamis: "); Serial.print(arah);
        Serial.print(" | spd="); Serial.println(lifterSpdValue, 3);
      } else {
        Serial.println("Lifter dinamis: STOP");
      }
    }

    // --- PRESET: gerak ke ketinggian tertentu (cm) ---
    // Mapping dari Python:
    //   X(0)  → 1.5 cm   Ambil dari lantai
    //   O(1)  → 8.0 cm   Meja Ruang Tamu / Sampah Kotak-2
    //   L3(8) → 5.5 cm   Lemari Makanan L1
    //   R3(9) → 10.0 cm  Meja Kamar / Sampah Kuning
    //   □(2)  → 13.0 cm  Meja Dapur / Sampah Hijau
    else if (dt.startsWith("PRESET,")) {
      float targetH = dt.substring(7).toFloat();
      lifterMode     = LIFTER_PRESET;
      lifterSpdValue = 0.0;
      Serial.print("PRESET ketinggian: ");
      Serial.print(targetH);
      Serial.println(" cm");
      moveToHeightB(targetH);
    }

    // --- HOMING: reset lifter B ke posisi atas (21.5 cm) ---
    // Dipicu oleh tombol Segitiga/△ (index 3) dari Python
    else if (dt.startsWith("HOMING")) {
      lifterMode     = LIFTER_PRESET;
      lifterSpdValue = 0.0;
      Serial.println("HOMING diperintahkan dari joystick");
      startHomingLifterB();
    }
    // --- PILIH PILOT (dari menu Python saat start) ---
    //   PILOT,ZAKY / PILOT,DAFFI → masuk ke gigi NORMAL pilot tsb
    else if (dt.startsWith("PILOT,")) {
      String name = dt.substring(6);
      int pilot = -1;
      if (name == "ZAKY")  pilot = PILOT_ZAKY;
      if (name == "DAFFI") pilot = PILOT_DAFFI;

      if (pilot >= 0) {
        if (EEPROM.read(ADDR_ACTIVE_PILOT) != pilot) EEPROM.write(ADDR_ACTIVE_PILOT, pilot);
        applyPreset(pilot, 0, true);
      } else {
        Serial3.print("Pilot tidak dikenal: "); Serial3.println(name);
      }
    }

// --- MODE KECEPATAN (Segitiga / X) — memakai preset pilot aktif ---
    else if (dt.startsWith("SPEED,")) {
      String mode = dt.substring(6);
      
      if (mode == "MAX") {
        applyPreset(currentPilot, 1, true);
      } 
      else if (mode == "NORMAL") {
        applyPreset(currentPilot, 0, true);
      }
    }

    // --- TUNING KECEPATAN: MAJU & STRAFE (Options / Share) ---
    else if (dt.startsWith("SPEED_ADJ,")) {
      String dir = dt.substring(10);
      if (dir == "UP") {
        boostMaju   += 0.1;
        boostStrafe += 0.1;
      } 
      else if (dir == "DOWN") {
        boostMaju   -= 0.1;
        boostStrafe -= 0.1;
      }

      // Batas aman
      if (boostMaju < 0.1) boostMaju = 0.1;
      if (boostStrafe < 0.1) boostStrafe = 0.1;

      saveCalibrationToEEPROM(); 
      
      Serial3.print("Speed Tuning -> Maju: "); Serial3.print(boostMaju, 1);
      Serial3.print(" | Strafe: "); Serial3.println(boostStrafe, 1);
    }

    // --- TUNING KECEPATAN PUTAR / OMEGA (L3 / R3) ---
    else if (dt.startsWith("OMEGA_ADJ,")) {
      String dir = dt.substring(10);
      if (dir == "UP") {
        omegaforces += 0.1; // R3 ditekan
      } 
      else if (dir == "DOWN") {
        omegaforces -= 0.1; // L3 ditekan
      }

      // Batas aman agar putaran tidak menjadi minus atau nol mutlak
      if (omegaforces < 0.1) omegaforces = 0.1;

      saveCalibrationToEEPROM(); // Simpan otomatis!
      
      Serial3.print("Tuning Putaran (Omega) menjadi: "); 
      Serial3.println(omegaforces, 1);
    }

    // --- LIVE TUNING KDEPAN (Kotak / Bulat) ---
    else if (dt.startsWith("KDEPAN,")) {
      String dir = dt.substring(7);
      if (dir == "UP")   kDepan += 0.02; 
      if (dir == "DOWN") kDepan -= 0.02; 
      
      saveCalibrationToEEPROM(); 
      
      Serial3.print("Tuning kDepan menjadi: ");
      Serial3.println(kDepan, 3);
    }

    // ========================================================
    // --- TAMBAHKAN KODE INI KEMBALI AGAR TOMBOL BERFUNGSI ---
    // ========================================================
    else if (dt.startsWith("BUTTON,") || dt.startsWith("HAT,")) {
      joystik(dt); // Melemparkan data ke Bluetooth_Function.ino
    }
  } // Penutup "if (stringComplete)"
}   // Penutup "void readSerial()"

void parseData(String d) {
  int xIndex = d.indexOf('X');
  int yIndex = d.indexOf('Y');
  int rIndex = d.indexOf('R');

  if (xIndex != -1 && yIndex != -1) {
    String xStr = d.substring(xIndex + 1, yIndex);
    String yStr;
    if (rIndex != -1) yStr = d.substring(yIndex + 1, rIndex);
    else yStr = d.substring(yIndex + 1);

    float newX = xStr.toFloat() * 255;
    float newY = yStr.toFloat() * 255;

    if (abs(newX - prev_xValue) > 2 || abs(newY - prev_yValue) > 2) {
      xValue = newX;
      yValue = newY; 
      prev_xValue = xValue;
      prev_yValue = yValue;

      global_X1 = mapNilai(xValue, -255, 255, -1.0, 1.0);
      global_Y1 = mapNilai(yValue, -255, 255, -1.0, 1.0);
      global_Y1 = -global_Y1;

      if (abs(global_X1) < 0.1) global_X1 = 0;
      if (abs(global_Y1) < 0.1) global_Y1 = 0;

      if (global_X1 == 0 && global_Y1 == 0) {
        Vx = 0; Vy = 0;
      } else {
        Convertion();
      }

      Serial.print("Translasi  X: "); Serial.print(xValue);
      Serial.print("  Y: "); Serial.println(yValue);
    }
  }

  if (rIndex != -1) {
    String rStr = d.substring(rIndex + 1);
    float newR = rStr.toFloat() * 255;
    global_X2 = mapNilai(newR, -255, 255, -0.5, 0.5);
    if (abs(global_X2) < 0.1) global_X2 = 0;
  }
}

void Convertion() {     
  if (global_X1 < 0.01 && global_X1 > -0.01) { global_X1 = 0; }
  if (global_Y1 < 0.01 && global_Y1 > -0.01) { global_Y1 = 0; }
  if (global_X2 < 0.01 && global_X2 > -0.01) { global_X2 = 0; }

  float speedMultiplier = 1.5;
  
  Vx    = global_X1;
  Vy    = global_Y1;
  Omega = global_X2 * 5;
}

void PID1(float setpoint, float feedback) {
  float kp = 2.2, ki = 15, kd = 0;
  static unsigned long prevT = 0;
  static float prevE = 0.0, integral = 0.0;     

  long currT = micros();
  float deltaT = ((float)(currT - prevT)) / (1.0e6);
  if (deltaT <= 0.0001) deltaT = 0.0001;

  float e    = setpoint - feedback;
  float dedt = (e - prevE) / deltaT;
  integral  += e * deltaT;
  pidOutput[1] = (kp * e + kd * dedt + ki * integral);
  if (setpoint < 0.5 && setpoint > -0.5) { integral = 0; pidOutput[1] = 0; } // Sesuaikan nomor pidOutput[1], [2], [3]

  prevT = currT;
  prevE = e;
}

void PID2(float setpoint, float feedback) {
  float kp = 2.2, ki = 15, kd = 0;
  static unsigned long prevT = 0;
  static float prevE = 0.0, integral = 0.0;     

  long currT = micros();
  float deltaT = ((float)(currT - prevT)) / (1.0e6);
  if (deltaT <= 0.0001) deltaT = 0.0001;

  float e    = setpoint - feedback;
  float dedt = (e - prevE) / deltaT;
  integral  += e * deltaT;
  pidOutput[2] = (kp * e + kd * dedt + ki * integral);
if (setpoint < 0.5 && setpoint > -0.5) { integral = 0; pidOutput[2] = 0; } // Sesuaikan nomor pidOutput[1], [2], [3]

  prevT = currT;
  prevE = e;
}

void PID3(float setpoint, float feedback) {
  float kp = 2.2, ki = 15, kd = 0;
  static unsigned long prevT = 0;
  static float prevE = 0.0, integral = 0.0;     

  long currT = micros();
  float deltaT = ((float)(currT - prevT)) / (1.0e6);
  if (deltaT <= 0.0001) deltaT = 0.0001;

  float e    = setpoint - feedback;
  float dedt = (e - prevE) / deltaT;
  integral  += e * deltaT;
  pidOutput[3] = (kp * e + kd * dedt + ki * integral);
if (setpoint < 0.5 && setpoint > -0.5) { integral = 0; pidOutput[3] = 0; } // Sesuaikan nomor pidOutput[1], [2], [3]

  prevT = currT;
  prevE = e;
}

void controlMotor() {
  PID1(Vmot[1], velocityEncoder[1]);
  PID2(Vmot[2], velocityEncoder[2]);
  PID3(Vmot[3], velocityEncoder[3]);

  const int motorR[4] = {0, motor1R, motor2R, motor3R}; //
  const int motorL[4] = {0, motor1L, motor2L, motor3L}; //

for (int i = 1; i < 4; i++) {
    // Ubah batas 3000 menjadi 1500 (atau 1200 jika ingin lebih agresif)
    float maxPidScale = 1500.0; 

    if (pidOutput[i] > maxPidScale)       pidOutput[i] = maxPidScale;
    else if (pidOutput[i] < -maxPidScale) pidOutput[i] = -maxPidScale;

    mutlakPidOutput[i] = abs(pidOutput[i]);
    PWM[i]       = mapNilai(mutlakPidOutput[i], 0, maxPidScale, 0, 255);


    // Mencegah Overflow PWM
    if (PWM[i] > 255) {
      PWM[i] = 255;
    }
    if (PWM[i] > 0 && PWM[i] < 15) {
      PWM[i] = 0; 
    }
    // ==========================================

    if (pidOutput[i] > 0) { //
      analogWrite(motorR[i], PWM[i]); //
      analogWrite(motorL[i], 0); //
    } else if (pidOutput[i] < 0) { //
      analogWrite(motorR[i], 0); //
      analogWrite(motorL[i], PWM[i]); //
    } else { //
      analogWrite(motorR[i], 0); //
      analogWrite(motorL[i], 0); //
    }
  }
}