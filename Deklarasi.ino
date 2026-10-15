void inisialisasi(){
  myServo1.attach(8); 
  myServo2.attach(6);
  Serial.begin(9600);
  Serial3.begin(9600);
  // bluetooth.begin("RESP Robot Transporter");
  
  stepperB.setMaxSpeed(2500);
  stepperB.setAcceleration(1200);

  // Konfigurasi pin sebagai input
  pinMode(ENC1A, INPUT);
  pinMode(ENC1B, INPUT);
  pinMode(ENC2A, INPUT);
  pinMode(ENC2B, INPUT);
  pinMode(ENC3A, INPUT);
  pinMode(ENC3B, INPUT);

  pinMode(LimitA, INPUT_PULLUP);
  pinMode(LimitB, INPUT_PULLUP);

  pinMode(motor1R, OUTPUT);
  pinMode(motor1L, OUTPUT);
  pinMode(motor2R, OUTPUT);
  pinMode(motor2L, OUTPUT);
  pinMode(motor3R, OUTPUT);
  pinMode(motor3L, OUTPUT);
  pinMode(DIR_PINA, OUTPUT);
  pinMode(STEP_PINA, OUTPUT);
  pinMode(DIR_PINB, OUTPUT);
  pinMode(STEP_PINB, OUTPUT);
  // Mengaktifkan interrupt untuk pin A
  attachInterrupt(digitalPinToInterrupt(ENC1A), readEncoder1, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC2A), readEncoder2, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC3A), readEncoder3, CHANGE);

  myServo1.write(60);
  myServo2.write(13); //menutup gripper2
  
  //homingLifterA();
  // homingLifterB();
  startHomingLifterB();

  

}

float mapNilai(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}