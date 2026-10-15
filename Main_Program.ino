void setup() {
  inisialisasi();
  loadCalibrationFromEEPROM();
}

void loop() {
  // --- Homing state machine (selalu jalan) ---
  updateHomingLifterB();

  // --- Stepper A: selalu pakai moveTo + run ---
  stepperA.run();

  // --- Stepper B: mode tergantung perintah lifter ---
  //   LIFTER_PRESET  → stepperB.run() (AccelStepper moveTo)
  //   LIFTER_DYNAMIC → updateLifterDynamic() pakai runSpeed()
  //                    stepperB.run() TIDAK dipanggil agar tidak konflik
  if (lifterMode == LIFTER_PRESET) {
    stepperB.run();
  }
  updateLifterDynamic();

  // --- Input & kontrol robot ---
  readSerial();          // baca perintah dari joystick via BT
  encoderToVelocity();   // baca encoder roda
  inversKinematic();     // hitung target RPM roda
  controlMotor();        // PID + kirim PWM ke driver motor
}
