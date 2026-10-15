// ============================================================
// Helper: baca tinggi aktual dari posisi stepper
// (Dibiarkan agar fungsi preset lain tidak error)
// ============================================================
float getActualHeightB() {
  return 21.5 + (float)stepperB.currentPosition() / steps_per_cm;
}

// ============================================================
// moveToHeightA — tidak berubah
// ============================================================
void moveToHeightA(float target_heightA) {
  if (target_heightA >= min_height && target_heightA <= max_height) {
    Serial.print("Stepper A menuju ");
    Serial.print(target_heightA);
    Serial.println(" cm");
    target_heightA = target_heightA - Height_Grounds;
    long target_steps = target_heightA * steps_per_cm;
    stepperA.moveTo(-target_steps);
    current_heightA = target_heightA;
  }
}

// ============================================================
// moveToHeightB — (Mode Preset Tinggi)
// Jika masih ingin pakai tombol Kotak/Bulat, fungsi ini tetap jalan.
// ============================================================
void moveToHeightB(float target_heightB) {
  if (target_heightB >= min_height && target_heightB <= max_height) {
    Serial.print("Stepper B menuju ");
    Serial.print(target_heightB);
    Serial.println(" cm");

    stepperB.setMaxSpeed(3500);
    stepperB.setAcceleration(1800);

    // Absolute: pos=0 = 21.5 cm, turun = negatif
    long target_steps = (long)((target_heightB - 21.5) * steps_per_cm);
    stepperB.moveTo(target_steps);

    // Update referensi tinggi
    current_heightB = target_heightB;
  }
}

// ============================================================
// Homing Lifter B — DINONAKTIFKAN
// (Dibiarkan kosong agar tab Main_Program tidak error saat kompilasi)
// ============================================================
void startHomingLifterB() {
  Serial.println("Fitur Homing dinonaktifkan. Mode Full Analog.");
}

void updateHomingLifterB() {
  // Kosong - Tidak ada lagi pembacaan Limit Switch
}

// ============================================================
// updateLifterDynamic — FULL MANUAL ANALOG KANAN
// Limit otomatis (atTop / atBottom) dihapus!
// Motor akan terus bergerak selama stik ditahan.
// ============================================================
void updateLifterDynamic() {
  if (lifterMode != LIFTER_DYNAMIC) return;

  // Jika stik analog di tengah (diam), matikan kecepatan
  if (lifterSpdValue == 0.0) {
    stepperB.setSpeed(0);
    stepperB.runSpeed();
    
    // Sinkronisasi posisi agar jika ganti ke mode Preset,
    // titik acuannya tidak melompat.
    current_heightB = getActualHeightB();
    return;
  }

  // Kecepatan proporsional murni berdasarkan tekanan stik
  // Positif = naik, Negatif = turun
  float speed = lifterSpdValue * MAX_DYNAMIC_SPEED;
  stepperB.setSpeed(speed);
  stepperB.runSpeed();

  // Sinkronisasi posisi terus-menerus
  current_heightB = getActualHeightB();
}