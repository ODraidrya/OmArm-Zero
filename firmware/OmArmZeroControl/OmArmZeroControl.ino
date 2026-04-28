/*
 * OmArm Zero Control Firmware
 * Für 6-DOF (5+1 Gripper) Roboterarm (6 Servos)
 * 
 * Hardware:
 * - ESP32 DevKit
 * - PCA9685 16-Channel PWM Driver (I2C: SDA=21, SCL=22)
 * - 3x MG996R Servo (J1, J2, J3)
 * - 3x SG90 Micro Servo (J4, J5, Gripper/J6)
 * 
 * Protokoll:
 * - Baud: 115200
 * - Commands:
 *   MOVE:d1,d2,d3,d4,d5,d6\n      - Move to positions (0-180°)
 *   HOME\n                        - Move to home position (0°)
 *   STATUS\n                      - Return current positions
 */

#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

// PCA9685 Setup
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);

// Servo Configuration
#define NUM_SERVOS 6
// Channels für J1-J6 (Beibehalten der ersten 6 Kanäle des alten Codes)
const uint8_t SERVO_CHANNELS[NUM_SERVOS] = {0, 3, 4, 7, 8, 11};
const char* SERVO_NAMES[NUM_SERVOS] = {"J1", "J2", "J3", "J4", "J5", "Gripper"};

// PWM Pulse Width (us) for 0° and 180°
// MG996R (J1-J3): 500-2500us
// SG90 (J4-J6): 500-2400us
const uint16_t SERVO_MIN_PULSE[NUM_SERVOS] = {500, 500, 500, 500, 500, 500};
const uint16_t SERVO_MAX_PULSE[NUM_SERVOS] = {2500, 2500, 2500, 2400, 2400, 2400};

// Current positions (degrees 0-180)
int currentPos[NUM_SERVOS] = {0, 0, 0, 0, 0, 0};
int targetPos[NUM_SERVOS] = {0, 0, 0, 0, 0, 0};

// Safety limits (degrees)
const int MIN_ANGLE = 0;
const int MAX_ANGLE = 180;

// Movement speed (degrees per update cycle)
int movementSpeed = 5;
int updateIntervalMs = 20;  // 50Hz update rate

// Timing
unsigned long lastUpdateTime = 0;
unsigned long lastStatusTime = 0;

// Serial buffer
String serialBuffer = "";

void setup() {
  // Serial setup
  Serial.begin(115200);
  while (!Serial && millis() < 3000); // Wait max 3 seconds
  
  Serial.println("\n╔════════════════════════════════════════════╗");
  Serial.println("║  OmArm Zero Control Firmware             ║");
  Serial.println("║  Konfig: 6-DOF (5+1 Gripper, 6 Servos)   ║");
  Serial.println("╚════════════════════════════════════════════╝");
  
  // I2C Setup
  Wire.begin(21, 22); // SDA=21, SCL=22
  delay(100);
  
  // PCA9685 Setup
  pwm.begin();
  pwm.setOscillatorFrequency(27000000);
  pwm.setPWMFreq(50); // 50Hz for servos
  delay(100);
  
  Serial.println("✓ PCA9685 initialisiert");
  
  // Move to home position
  Serial.println("→ Fahre zu Home Position (0°)...");
  for (int i = 0; i < NUM_SERVOS; i++) {
    setServoAngle(i, 0);
    currentPos[i] = 0;
    targetPos[i] = 0;
    delay(50);
  }
  
  Serial.println("✓ System bereit!");
  Serial.println("Befehle: MOVE:d1,d2,d3,d4,d5,d6 | HOME | STATUS");
  Serial.println("─────────────────────────────────────────────");
}

void loop() {
  unsigned long now = millis();
  
  // Read serial commands
  readSerialCommands();
  
  // Smooth movement update at fixed rate
  if (now - lastUpdateTime >= updateIntervalMs) {
    lastUpdateTime = now;
    updateServoMovement();
  }
  
  // Periodic status (every 5 seconds if no commands)
  if (now - lastStatusTime > 5000) {
    lastStatusTime = now;
    // Uncomment for debug:
    // printStatus();
  }
}

void readSerialCommands() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    
    if (c == '\n' || c == '\r') {
      if (serialBuffer.length() > 0) {
        processCommand(serialBuffer);
        serialBuffer = "";
      }
    } else {
      serialBuffer += c;
      
      // Safety: Prevent buffer overflow
      if (serialBuffer.length() > 128) {
        Serial.println("ERROR: Command too long");
        serialBuffer = "";
      }
    }
  }
}

void processCommand(String cmd) {
  cmd.trim();
  
  if (cmd.length() == 0) return;
  
  if (cmd.startsWith("MOVE:")) {
    handleMoveCommand(cmd.substring(5));
  }
  else if (cmd.startsWith("SPEED:")) {
    handleSpeedCommand(cmd.substring(6));
  }
  else if (isdigit(cmd.charAt(0))) {
    handleSingleJointCommand(cmd);
  }
  else if (cmd.equals("HOME")) {
    handleHomeCommand();
  }
  else if (cmd.equals("STATUS")) {
    handleStatusCommand();
  }
  else {
    Serial.print("ERROR: Unknown command: ");
    Serial.println(cmd);
  }
}

void handleSingleJointCommand(String cmd) {
  int commaIndex = cmd.indexOf(',');
  if (commaIndex == -1) return;
  
  int id = cmd.substring(0, commaIndex).toInt();
  int angle = cmd.substring(commaIndex + 1).toInt();
  
  // Convert 1-based ID to 0-based index
  int index = id - 1;
  
  if (index >= 0 && index < NUM_SERVOS) {
    angle = constrain(angle, MIN_ANGLE, MAX_ANGLE);
    targetPos[index] = angle;
  }
}

void handleMoveCommand(String params) {
  int values[NUM_SERVOS];
  int count = 0;
  
  // Parse comma-separated values
  int startIdx = 0;
  for (int i = 0; i <= params.length() && count < NUM_SERVOS; i++) {
    if (i == params.length() || params.charAt(i) == ',') {
      String val = params.substring(startIdx, i);
      val.trim();
      
      if (val.length() > 0) {
        values[count] = val.toInt();
        count++;
      }
      startIdx = i + 1;
    }
  }
  
  // Validate we got all values
  if (count != NUM_SERVOS) {
    Serial.print("ERROR: Expected ");
    Serial.print(NUM_SERVOS);
    Serial.print(" values, got ");
    Serial.println(count);
    return;
  }
  
  // Apply safety limits and set target positions
  for (int i = 0; i < NUM_SERVOS; i++) {
    int angle = constrain(values[i], MIN_ANGLE, MAX_ANGLE);
    
    if (angle != values[i]) {
      Serial.print("WARN: ");
      Serial.print(SERVO_NAMES[i]);
      Serial.print(" clamped ");
      Serial.print(values[i]);
      Serial.print("° → ");
      Serial.print(angle);
      Serial.println("°");
    }
    
    targetPos[i] = angle;
  }
  
  // Acknowledge
  Serial.println("OK: MOVE");
}

void handleSpeedCommand(String params) {
  int commaIndex = params.indexOf(',');
  if (commaIndex == -1) return;
  
  int newSpeed = params.substring(0, commaIndex).toInt();
  int newInterval = params.substring(commaIndex + 1).toInt();
  
  // Safety checks
  if (newSpeed >= 1 && newSpeed <= 20 && newInterval >= 5 && newInterval <= 50) {
    movementSpeed = newSpeed;
    updateIntervalMs = newInterval;
  }
}

void handleHomeCommand() {
  Serial.println("→ HOME");

  for (int i = 0; i < NUM_SERVOS; i++) {
    targetPos[i] = 0;
  }
  
  Serial.println("OK: HOME");
}

void handleStatusCommand() {
  Serial.print("STATUS:");
  for (int i = 0; i < NUM_SERVOS; i++) {
    Serial.print(currentPos[i]);
    if (i < NUM_SERVOS - 1) Serial.print(",");
  }
  Serial.println();
}

void updateServoMovement() {
  for (int i = 0; i < NUM_SERVOS; i++) {
    if (currentPos[i] != targetPos[i]) {
      
      // Move towards target with speed limit
      int diff = targetPos[i] - currentPos[i];
      int step = constrain(diff, -movementSpeed, movementSpeed);
      
      currentPos[i] += step;
      setServoAngle(i, currentPos[i]);
    }
  }
}

void setServoAngle(uint8_t servoIndex, int angle) {
  if (servoIndex >= NUM_SERVOS) return;
  
  // Clamp angle
  angle = constrain(angle, MIN_ANGLE, MAX_ANGLE);
  
  // Map angle to pulse width
  uint16_t minPulse = SERVO_MIN_PULSE[servoIndex];
  uint16_t maxPulse = SERVO_MAX_PULSE[servoIndex];
  uint16_t pulseWidth = map(angle, 0, 180, minPulse, maxPulse);
  
  // Convert to PCA9685 PWM value (4096 steps at 50Hz = 20ms)
  uint16_t pwmValue = (uint32_t)pulseWidth * 4096 / 20000;
  
  // Send to PCA9685
  pwm.setPWM(SERVO_CHANNELS[servoIndex], 0, pwmValue);
}

void printStatus() {
  Serial.println("\n─── Status ───");
  for (int i = 0; i < NUM_SERVOS; i++) {
    Serial.print(SERVO_NAMES[i]);
    Serial.print(": ");
    Serial.print(currentPos[i]);
    Serial.print("° → ");
    Serial.print(targetPos[i]);
    Serial.println("°");
  }
  Serial.println("──────────────");
}
