/*
 * Calibration — Send All Servos to Home (0°)
 * OmArm Zero — 6-DOF (5+1 Gripper)
 *
 * Moves all six servos to 0° and holds them there.
 * Use this BEFORE attaching the horn:
 *   1. Upload this sketch
 *   2. Power the servo rail (5V 10A supply)
 *   3. Wait until each servo stops vibrating
 *   4. Press the horn onto the spline in neutral position
 *   5. Tighten the horn screw
 *   6. Repeat for all six servos
 *
 * Wiring:
 *   ESP32 GPIO 21 (SDA) -> PCA9685 SDA
 *   ESP32 GPIO 22 (SCL) -> PCA9685 SCL
 *   PCA9685 V+          -> 5V 10A DC supply
 *   Servos on channels:  CH0, CH3, CH4, CH7, CH8, CH11
 *
 * Open Serial Monitor at 115200 baud.
 */

#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

// PCA9685
#define SDA_PIN   21
#define SCL_PIN   22
#define PCA_ADDR  0x40
#define PWM_FREQ  50       // 50 Hz for servos

// Servo pulse range (microseconds)
#define SERVO_MIN_US  500  // 0°
#define SERVO_MAX_US  2500 // 180°
#define HOME_ANGLE    0

// Joint-to-channel mapping (same as main firmware)
// J1=Base, J2=Shoulder, J3=Elbow, J4=WristPitch, J5=WristRoll, J6=Gripper
const int NUM_JOINTS = 6;
const int jointChannels[NUM_JOINTS] = { 0, 3, 4, 7, 8, 11 };
const char* jointNames[NUM_JOINTS] = {
  "J1 Base",
  "J2 Shoulder",
  "J3 Elbow",
  "J4 Wrist Pitch",
  "J5 Wrist Roll",
  "J6 Gripper"
};

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(PCA_ADDR);

// Convert angle (0-180) to PCA9685 PWM ticks
uint16_t angleToPWM(int angle) {
  int pulseUs = map(angle, 0, 180, SERVO_MIN_US, SERVO_MAX_US);
  // PCA9685 has 4096 ticks per 20 ms period
  return (uint16_t)((pulseUs * 4096L) / 20000L);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }

  Serial.println();
  Serial.println("==========================================");
  Serial.println("  OmArm Zero — Calibration: Send to Home");
  Serial.println("  All servos -> 0 degrees");
  Serial.println("==========================================");
  Serial.println();

  Wire.begin(SDA_PIN, SCL_PIN);
  pwm.begin();
  pwm.setOscillatorFrequency(25000000);
  pwm.setPWMFreq(PWM_FREQ);
  delay(100);

  uint16_t ticks = angleToPWM(HOME_ANGLE);
  Serial.print("Home angle: ");
  Serial.print(HOME_ANGLE);
  Serial.print(" deg  ->  PWM ticks: ");
  Serial.println(ticks);
  Serial.println();

  // Move each servo to 0°
  for (int i = 0; i < NUM_JOINTS; i++) {
    pwm.setPWM(jointChannels[i], 0, ticks);
    Serial.print("  ");
    Serial.print(jointNames[i]);
    Serial.print("  (CH");
    Serial.print(jointChannels[i]);
    Serial.println(")  -> 0 deg");
    delay(300);  // stagger to avoid current spike
  }

  Serial.println();
  Serial.println("All servos are now at 0 degrees.");
  Serial.println("Wait until each servo stops vibrating,");
  Serial.println("then attach the horn in neutral position.");
  Serial.println();
  Serial.println("Type 'r' + Enter to re-send 0 deg to all servos.");
}

void loop() {
  // Allow re-sending home position via Serial
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'r' || c == 'R') {
      Serial.println();
      Serial.println("Re-sending 0 deg to all servos ...");
      uint16_t ticks = angleToPWM(HOME_ANGLE);
      for (int i = 0; i < NUM_JOINTS; i++) {
        pwm.setPWM(jointChannels[i], 0, ticks);
        delay(300);
      }
      Serial.println("Done.");
    }
  }
}
