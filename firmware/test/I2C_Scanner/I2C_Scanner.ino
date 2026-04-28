/*
 * I2C Scanner for OmArm Zero
 * Scans all I2C addresses and reports devices found.
 *
 * Expected result: PCA9685 at address 0x40
 *
 * Wiring:
 *   ESP32 GPIO 21 (SDA) -> PCA9685 SDA
 *   ESP32 GPIO 22 (SCL) -> PCA9685 SCL
 *   GND                 -> PCA9685 GND
 *   3.3V                -> PCA9685 VCC
 *
 * Open Serial Monitor at 115200 baud after uploading.
 * If 0x40 does NOT show up, check your SDA/SCL wiring
 * before moving on to the main firmware.
 */

#include <Wire.h>

#define SDA_PIN 21
#define SCL_PIN 22

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }

  Serial.println();
  Serial.println("=================================");
  Serial.println("  OmArm Zero — I2C Scanner");
  Serial.println("  SDA = GPIO 21, SCL = GPIO 22");
  Serial.println("=================================");
  Serial.println();

  Wire.begin(SDA_PIN, SCL_PIN);
}

void loop() {
  Serial.println("Scanning I2C bus ...");
  Serial.println();

  int devicesFound = 0;

  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    byte error = Wire.endTransmission();

    if (error == 0) {
      devicesFound++;
      Serial.print("  Device found at 0x");
      if (addr < 16) Serial.print("0");
      Serial.print(addr, HEX);

      // Label known addresses
      if (addr == 0x40) {
        Serial.print("  <-- PCA9685 (default)");
      } else if (addr == 0x70) {
        Serial.print("  <-- PCA9685 all-call");
      }
      Serial.println();
    }
  }

  Serial.println();
  if (devicesFound == 0) {
    Serial.println("!! No I2C devices found !!");
    Serial.println("   Check wiring: SDA -> GPIO 21, SCL -> GPIO 22");
    Serial.println("   Check power:  VCC -> 3.3V, GND -> GND");
  } else {
    Serial.print("Done. ");
    Serial.print(devicesFound);
    Serial.println(" device(s) found.");
    if (devicesFound >= 1) {
      Serial.println("If 0x40 is listed, your PCA9685 is ready.");
    }
  }

  Serial.println();
  Serial.println("Next scan in 5 seconds ...");
  Serial.println("---");
  delay(5000);
}
