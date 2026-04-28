/*
 * OmArm Zero LittleFS Firmware
 * 6-DOF (5+1 Gripper) Robot Arm (6 Servos) with Web Server + LittleFS
 * 
 * This version stores web files on the ESP32 filesystem (LittleFS).
 * Allows including large files like images.
 * 
 * Hardware:
 * - ESP32 DevKit
 * - PCA9685 16-Channel PWM Driver (I2C: SDA=21, SCL=22)
 * - 3x MG996R Servo (J1, J2, J3)
 * - 3x SG90 Micro Servo (J4, J5, Gripper/J6)
 * 
 * Upload Instructions:
 * 1. Install "ESP32 Sketch Data Upload" tool (Arduino 1.8) or use LittleFS plugin
 * 2. Tools -> ESP32 Sketch Data Upload (uploads /data folder)
 * 3. Upload this sketch normally
 * 
 * WiFi Access Point: "OmArmZero" (Password: "omarmcontrol")
 * Web Interface: http://10.10.10.1
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// WiFi Configuration
const char* ssid = "OmArmZero";
const char* password = "omarmcontrol";

// Web Server
WebServer server(80);

// PCA9685 Setup
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);

// Servo Config
#define NUM_SERVOS 6
const uint8_t SERVO_CHANNELS[NUM_SERVOS] = {0, 3, 4, 7, 8, 11};
const uint16_t SERVO_MIN_PULSE[NUM_SERVOS] = {500, 500, 500, 500, 500, 500};
const uint16_t SERVO_MAX_PULSE[NUM_SERVOS] = {2500, 2500, 2500, 2400, 2400, 2400};

// State
int currentPos[NUM_SERVOS] = {0, 0, 0, 0, 0, 0};
int targetPos[NUM_SERVOS] = {0, 0, 0, 0, 0, 0};
int movementSpeed = 5;
unsigned long lastUpdateTime = 0;

// Function declarations
void setServoAngle(uint8_t servoIndex, int angle);
void updateServoMovement();
String getContentType(String filename);

// ------------------------------------------------------------------
// Web Server Handlers
// ------------------------------------------------------------------

void handleCommand() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"Body missing\"}");
    return;
  }
  
  String body = server.arg("plain");
  DynamicJsonDocument doc(512);
  if (deserializeJson(doc, body)) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }

  String cmd = doc["cmd"];
  
  // STATUS check
  if (cmd.startsWith("STATUS")) {
    server.send(200, "application/json", "{\"status\":\"ok\"}");
    return;
  }
  
  // Joint Command: "1,90"
  if (isdigit(cmd.charAt(0))) {
    int comma = cmd.indexOf(',');
    if (comma != -1) {
      int id = cmd.substring(0, comma).toInt();
      int angle = cmd.substring(comma+1).toInt();
      if (id >= 1 && id <= 6) {
        targetPos[id-1] = constrain(angle, 0, 180);
      }
    }
  }
  // HOME
  else if (cmd.equals("HOME")) {
    for (int i = 0; i < 6; i++) targetPos[i] = 0;
  }
  // SPEED
  else if (cmd.startsWith("SPEED:")) {
    int comma = cmd.indexOf(',');
    if (comma != -1) {
      movementSpeed = cmd.substring(6, comma).toInt();
    }
  }
  // MOVE (Sequence Playback): "MOVE:90,90,90,90,90,90"
  else if (cmd.startsWith("MOVE:")) {
    String angles = cmd.substring(5);
    int idx = 0, start = 0;
    for (int i = 0; i <= angles.length() && idx < 6; i++) {
      if (i == angles.length() || angles.charAt(i) == ',') {
        targetPos[idx++] = constrain(angles.substring(start, i).toInt(), 0, 180);
        start = i + 1;
      }
    }
  }
  
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

// Serve files from LittleFS
void handleFileRequest() {
  String path = server.uri();
  
  if (path.endsWith("/")) {
    path += "index.html";
  }
  
  // Handle URL-encoded spaces
  path.replace("%20", " ");
  
  String contentType = getContentType(path);
  
  if (LittleFS.exists(path)) {
    File file = LittleFS.open(path, "r");
    server.streamFile(file, contentType);
    file.close();
  } else {
    server.send(404, "text/plain", "File Not Found: " + path);
  }
}

String getContentType(String filename) {
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".json")) return "application/json";
  else if (filename.endsWith(".png")) return "image/png";
  else if (filename.endsWith(".jpg") || filename.endsWith(".jpeg")) return "image/jpeg";
  else if (filename.endsWith(".svg")) return "image/svg+xml";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  return "text/plain";
}

// ------------------------------------------------------------------
// Setup & Loop
// ------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  
  // Initialize LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed!");
    return;
  }
  Serial.println("LittleFS mounted successfully");
  
  // List files (debug)
  File root = LittleFS.open("/");
  File file = root.openNextFile();
  while (file) {
    Serial.print("  File: ");
    Serial.print(file.name());
    Serial.print(" (");
    Serial.print(file.size());
    Serial.println(" bytes)");
    file = root.openNextFile();
  }
  
  // I2C & PWM
  Wire.begin(21, 22);
  pwm.begin();
  pwm.setOscillatorFrequency(27000000);
  pwm.setPWMFreq(50);
  delay(100);

  // WiFi AP
  WiFi.softAP(ssid, password);
  Serial.println("\n=================================");
  Serial.println("OmArmZero LittleFS Web Server");
  Serial.println("=================================");
  Serial.println("WiFi AP: " + String(ssid));
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());
  Serial.println("=================================");

  // API endpoint
  server.on("/api/command", HTTP_POST, handleCommand);
  
  // Serve all other requests from LittleFS
  server.onNotFound(handleFileRequest);
  
  server.enableCORS(true);
  server.begin();
}

void loop() {
  server.handleClient();
  
  unsigned long now = millis();
  if (now - lastUpdateTime >= 20) {
    lastUpdateTime = now;
    updateServoMovement();
  }
}

// ------------------------------------------------------------------
// Servo Control
// ------------------------------------------------------------------

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

void setServoAngle(uint8_t servoIndex, int angle) {
  if (servoIndex >= NUM_SERVOS) return;
  angle = constrain(angle, 0, 180);
  uint16_t pwmValue = map(angle, 0, 180, SERVO_MIN_PULSE[servoIndex], SERVO_MAX_PULSE[servoIndex]);
  pwmValue = (uint32_t)pwmValue * 4096 / 20000;
  pwm.setPWM(SERVO_CHANNELS[servoIndex], 0, pwmValue);
}
