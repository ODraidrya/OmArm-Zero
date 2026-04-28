/*
 * OmArm Zero ROS2 Serial Firmware
 *
 * Derived from OmArmZero_LittleFS.ino command and motion behavior,
 * but without WiFi/Web/LittleFS to keep the ROS2 serial path minimal.
 *
 * Supported commands:
 * - STATUS
 * - HOME
 * - ZERO
 * - MOVE:a1,a2,a3,a4,a5,a6
 * - SPEED:n
 * - i,angle    (single joint, i in 1..6)
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);

#define NUM_SERVOS 6

const uint8_t SERVO_CHANNELS[NUM_SERVOS] = {0, 3, 4, 7, 8, 11};
const uint16_t SERVO_MIN_PULSE[NUM_SERVOS] = {500, 500, 500, 500, 500, 500};
const uint16_t SERVO_MAX_PULSE[NUM_SERVOS] = {2500, 2500, 2500, 2400, 2400, 2400};

// Startup pose aligned to the MoveIt/RViz mapping for this calibration.
int currentPos[NUM_SERVOS] = {0, 0, 0, 0, 0, 0};
int targetPos[NUM_SERVOS] = {0, 0, 0, 0, 0, 0};

// Same movement semantics as LittleFS firmware.
int movementSpeed = 5;
unsigned long lastUpdateTime = 0;

void setServoAngle(uint8_t servoIndex, int angle) {
  if (servoIndex >= NUM_SERVOS) return;
  angle = constrain(angle, 0, 180);

  uint16_t us = map(angle, 0, 180, SERVO_MIN_PULSE[servoIndex], SERVO_MAX_PULSE[servoIndex]);
  uint16_t ticks = (uint32_t)us * 4096 / 20000;
  pwm.setPWM(SERVO_CHANNELS[servoIndex], 0, ticks);
}

void applyHomePose() {
  for (int i = 0; i < NUM_SERVOS; i++) {
    targetPos[i] = 0;
  }
}

void applyZeroPose() {
  for (int i = 0; i < NUM_SERVOS; i++) {
    targetPos[i] = 0;
  }
}

void updateServoMovement() {
  for (int i = 0; i < NUM_SERVOS; i++) {
    if (currentPos[i] != targetPos[i]) {
      int diff = targetPos[i] - currentPos[i];
      int step = constrain(diff, -movementSpeed, movementSpeed);
      currentPos[i] += step;
      setServoAngle(i, currentPos[i]);
    }
  }
}

String processCommand(const String &cmdRaw) {
  String cmd = cmdRaw;
  cmd.trim();

  if (cmd.length() == 0) {
    return "{\"status\":\"error\",\"message\":\"empty command\"}";
  }

  if (cmd.equals("STATUS")) {
    String status = "{\"status\":\"ok\",\"pos\":[";
    for (int i = 0; i < NUM_SERVOS; i++) {
      status += String(currentPos[i]);
      if (i < NUM_SERVOS - 1) status += ",";
    }
    status += "]}";
    return status;
  }

  if (cmd.equals("HOME")) {
    applyHomePose();
    return "{\"status\":\"ok\"}";
  }

  if (cmd.equals("ZERO")) {
    applyZeroPose();
    return "{\"status\":\"ok\"}";
  }

  if (cmd.startsWith("SPEED:")) {
    int comma = cmd.indexOf(',');
    if (comma != -1) {
      movementSpeed = constrain(cmd.substring(6, comma).toInt(), 1, 20);
    } else {
      movementSpeed = constrain(cmd.substring(6).toInt(), 1, 20);
    }
    return "{\"status\":\"ok\"}";
  }

  if (cmd.startsWith("MOVE:")) {
    String angles = cmd.substring(5);
    int idx = 0;
    int start = 0;

    for (int i = 0; i <= angles.length() && idx < NUM_SERVOS; i++) {
      if (i == angles.length() || angles.charAt(i) == ',') {
        targetPos[idx++] = constrain(angles.substring(start, i).toInt(), 0, 180);
        start = i + 1;
      }
    }

    if (idx == NUM_SERVOS) {
      return "{\"status\":\"ok\"}";
    }
    return "{\"status\":\"error\",\"message\":\"MOVE expects 6 values\"}";
  }

  if (isdigit(cmd.charAt(0))) {
    int comma = cmd.indexOf(',');
    if (comma != -1) {
      int id = cmd.substring(0, comma).toInt();
      int angle = cmd.substring(comma + 1).toInt();
      if (id >= 1 && id <= 6) {
        targetPos[id - 1] = constrain(angle, 0, 180);
        return "{\"status\":\"ok\"}";
      }
    }
    return "{\"status\":\"error\",\"message\":\"invalid joint command\"}";
  }

  return "{\"status\":\"error\",\"message\":\"unknown command\"}";
}

void handleSerialCommands() {
  if (!Serial.available()) return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  if (cmd.length() == 0) return;

  String response = processCommand(cmd);
  Serial.println(response);
}

void setup() {
  Serial.begin(115200);

  Wire.begin(21, 22);
  pwm.begin();
  pwm.setOscillatorFrequency(27000000);
  pwm.setPWMFreq(50);
  delay(100);

  for (int i = 0; i < NUM_SERVOS; i++) {
    setServoAngle(i, currentPos[i]);
  }

  Serial.println("OmArmZero ROS2 Serial Firmware ready");
  Serial.println("Commands: STATUS | HOME | ZERO | MOVE:a1,a2,a3,a4,a5,a6 | SPEED:n | i,angle");
}

void loop() {
  handleSerialCommands();

  unsigned long now = millis();
  if (now - lastUpdateTime >= 80) {
    lastUpdateTime = now;
    updateServoMovement();
  }
}
