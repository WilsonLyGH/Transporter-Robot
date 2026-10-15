// ============================================================
// joystik() — handler BUTTON dan HAT dari Python
//
// Tombol preset lifter (0, 1, 2, 3, 8, 9) TIDAK lagi masuk
// ke sini karena Python mengirim "PRESET,<cm>" / "HOMING"
// langsung, ditangani di readSerial() (Motor_Function.ino).
//
// Yang masuk ke sini hanya:
//   index 4  → Putar KIRI  (L1)
//   index 5  → Putar KANAN (R1)
//   index 6  → Toggle kecepatan lambat/normal
//   index 7  → Lifter 1.0 cm (tombol Share/Select)
//   TRIG0    → Toggle Gripper 1 (L2)
//   TRIG1    → Toggle Gripper 2 (R2)
//   HAT      → D-Pad translasi manual
// ============================================================
void joystik(String data) {
  if (data.startsWith("BUTTON,")) {

    // --- Tombol biasa (bukan trigger) ---
    if (data.indexOf("TRIG") == -1) {
      int c1    = data.indexOf(',');
      int c2    = data.indexOf(',', c1 + 1);
      int index = data.substring(c1 + 1, c2).toInt();
      int state = data.substring(c2 + 1).toInt();

      // index 4 — L1: Putar KIRI
      if (index == 4) {
        if (state == 1) {
          Serial.println("Putar KIRI");
          Omega = omegaforces;
        } else {
          Omega = 0;
        }
      }

      // index 5 — R1: Putar KANAN
      if (index == 5) {
        if (state == 1) {
          Serial.println("Putar KANAN");
          Omega = -omegaforces;
        } else {
          Omega = 0;
        }
      }

      // index 7 — Share/Create: Lifter ke 1.0 cm (posisi sangat rendah)
      if (index == 7) {
        if (state == 1) {
          Serial.println("Tombol 7: lifter 1.0 cm");
        }
      }
    }

    // --- Tombol Trigger (L2 / R2) → toggle gripper ---
    else {
      int trigIdxStart = data.indexOf("TRIG") + 4;
      int commaPos     = data.indexOf(',', trigIdxStart);
      int trigIndex    = data.substring(trigIdxStart, commaPos).toInt();
      int trigState    = data.substring(commaPos + 1).toInt();

      // TRIG0 — L2: Toggle Gripper 1
      if (trigIndex == 0) {
        if (trigState == 1) {
          TrigButt[2] = !TrigButt[2];
          if (TrigButt[2]) {
            Serial.println("Gripper 1 OPEN");
            myServo2.write(180);
          } else {
            Serial.println("Gripper 1 CLOSE");
            myServo2.write(7);
          }
        } else {
          Serial.println("L2 dilepas");
        }
      }

      // TRIG1 — R2: Toggle Gripper 2
      if (trigIndex == 1) {
        if (trigState == 1) {
          TrigButt[1] = !TrigButt[1];
          if (TrigButt[1]) {
            Serial.println("Gripper 2 OPEN");
            myServo1.write(180);
          } else {
            Serial.println("Gripper 2 CLOSE");
            myServo1.write(13);
          }
        } else {
          Serial.println("R2 dilepas");
        }
      }
    }
  }

  // === Data D-Pad (HAT) ===
  else if (data.startsWith("HAT,")) {
    int c1   = data.indexOf(',');
    int c2   = data.indexOf(',', c1 + 1);
    int hatX = data.substring(c1 + 1, c2).toInt();
    int hatY = data.substring(c2 + 1).toInt();

    if (hatY == 1) {
      Serial.println("Panah Atas");
      Vy = Vforces;
    } else if (hatY == -1) {
      Serial.println("Panah Bawah");
      Vy = -Vforces;
    } else {
      Vy = 0;
    }

    if (hatX == -1) {
      Serial.println("Panah Kiri");
      Vx = -Vforces;
    } else if (hatX == 1) {
      Serial.println("Panah Kanan");
      Vx = Vforces;
    } else {
      Vx = 0;
    }

    if (hatX == 1  && hatY == 1)  Serial.println("Serong kanan atas");
    if (hatX == 1  && hatY == -1) Serial.println("Serong kanan bawah");
    if (hatX == -1 && hatY == 1)  Serial.println("Serong kiri atas");
    if (hatX == -1 && hatY == -1) Serial.println("Serong kiri bawah");
  }
}
