#include <AccelStepper.h>
#include <Servo.h>
#include <EEPROM.h>
Servo myServo;

#define motor1L 10 
#define motor1R 11    
#define motor2L 4
#define motor2R 5
#define motor3L 9
#define motor3R 12

#define ENC1A 19
#define ENC1B 18
#define ENC2A 3
#define ENC2B 2
#define ENC3A 20
#define ENC3B 21

#define DIR_PINA  40
#define STEP_PINA 41

#define DIR_PINB 48
#define STEP_PINB 49

#define LimitA 38
#define LimitB 36

#define PPR 217  
#define WHEEL_CIRCUMFERENCE 0.1885 

Servo myServo1;
Servo myServo2;

AccelStepper stepperA(AccelStepper::DRIVER, STEP_PINA, DIR_PINA);
AccelStepper stepperB(AccelStepper::DRIVER, STEP_PINB, DIR_PINB);

enum HomingStateB {
  HOMING_IDLE,
  HOMING_RUNNING,
  HOMING_DONE
};
HomingStateB homingStateB = HOMING_IDLE;

// ============================================================
// MODE LIFTER
//   LIFTER_PRESET  → moveTo() + run()   (perintah PRESET / HOMING)
//   LIFTER_DYNAMIC → setSpeed() + runSpeed()  (perintah LIFTER_SPD)
// ============================================================
enum LifterMode { LIFTER_PRESET, LIFTER_DYNAMIC };
LifterMode lifterMode = LIFTER_PRESET;

// Nilai kecepatan dari Python LIFTER_SPD  (-1.0 s/d +1.0)
// +1.0 = naik penuh, -1.0 = turun penuh, 0.0 = berhenti
float lifterSpdValue = 0.0;

// Kecepatan maks dynamic mode (steps/sec) — lebih lambat dari preset
// supaya fine-tune presisi; sesuaikan jika perlu
const float MAX_DYNAMIC_SPEED = 900.0;

long time_wheels, time1, time2;
float Vmot[4], Vy = 0, Vx = 0, Omega = 0, L = 0.5, radiusWheels = 0.03; 
long encoderPosition[4], pulsePerSecond[4], lastPulse[4];
float velocityEncoder[4], pidOutput[4], mutlakPidOutput[4];        
unsigned long lastMillis = 0, prevT; 
unsigned long lastSignalTime = 0;
int PWM[4], DiviedPWM[4];

// --- Parameter Mekanik ---
const float steps_per_cm  = 62.5;
const float min_height     = 0.5;
const float max_height     = 21.5;
const float Height_Grounds = 3.5;
// --- VARIABEL GLOBAL UNTUK LIVE TUNING & EEPROM ---
float boostMaju   = 2.5;  // START DI GIGI NORMAL
float boostStrafe = 1.6;  
float kDepan      = 0.18; // Nilai kDepan aktif saat ini
float omegaforces = 1.2;
// ============================================================
// PILOT & PRESET
//   Pilot 0 = ZAKY, Pilot 1 = DAFFI
//   Mode  0 = NORMAL, Mode 1 = MAX
// ============================================================
#define PILOT_ZAKY  0
#define PILOT_DAFFI 1
const char* PILOT_NAME[2] = {"ZAKY", "DAFFI"};

struct SpeedPreset {
  float kDepan;
  float bMaju;
  float bStrafe;
  float omega;
};

// presets[pilot][mode] — nilai default (dipakai jika EEPROM kosong)
SpeedPreset presets[2][2] = {
  // ZAKY:  NORMAL                   MAX
  { {0.12, 2.6, 2.4, 1.1}, {0.08, 3.4, 3.4, 1.6} },
  // DAFFI: NORMAL                   MAX
  { {0.42, 0.9, 0.6, 0.2}, {0.12, 4.2, 2.3, 0.8} }
};

// Alamat EEPROM (float = 4 byte). Tiap pilot memakai blok 32 byte:
//   ZAKY  : 0  - 31  (sama dengan alamat lama → kalibrasi lama tetap terpakai)
//   DAFFI : 32 - 63
// Layout dalam blok: kN 0, kM 4, bMajuN 8, bStrafeN 12, bMajuM 16, bStrafeM 20, omegaN 24, omegaM 28
const int EEPROM_PILOT_BLOCK = 32;
const int ADDR_ACTIVE_PILOT  = 64;  // 1 byte: pilot terakhir yang dipilih

int addrKDepan (int p, int m) { return p * EEPROM_PILOT_BLOCK + 0  + m * 4; }
int addrBMaju  (int p, int m) { return p * EEPROM_PILOT_BLOCK + 8  + m * 8; }
int addrBStrafe(int p, int m) { return p * EEPROM_PILOT_BLOCK + 12 + m * 8; }
int addrOmega  (int p, int m) { return p * EEPROM_PILOT_BLOCK + 24 + m * 4; }

// Status pilot & mode kecepatan aktif
int currentPilot     = PILOT_ZAKY;
int currentSpeedMode = 0; // 0 = NORMAL, 1 = MAX
// -----------------------------------------
// Variabel stick
float Vforces = 1.4;
bool TrigButt[5];

// current_heightB selalu disinkronkan dari stepperB.currentPosition()
// Referensi: pos=0 => tinggi 21.5 cm (posisi homing/atas)
float current_heightB = 21.5;
float current_heightA = 21.5;
float target_heightA, target_heightB;
int prev_xValue = 0;
int prev_yValue = 0;

String inputString = "";
bool stringComplete = false;
int lx, ly, rx, ry;

int xValue = 127;
int yValue = 127;

// Var joystick
int varButton, conButton;
float global_X1, global_Y1, global_X2, global_Y2, Yaw;
float maxspeed = 3;

// Forward declarations
float mapNilai(float x, float in_min, float in_max, float out_min, float out_max);
void readEncoder1(void);
void readEncoder2(void);
void readEncoder3(void);
void inversKinematic(void);
void encoderToVelocity(void);
void PID1(float setpoint, float feedback);
void controlMotor(void);
void serialEvent(void);
void readSerial(void);
void parseData(String data);
void Convertion(void);
void updateLifterDynamic(void);
void applyPreset(int pilot, int mode, bool verbose);
void saveCalibrationToEEPROM(void);
void loadCalibrationFromEEPROM(void);
