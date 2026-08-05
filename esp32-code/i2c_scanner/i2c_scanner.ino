/*
 * ESP32 I2C 设备地址扫描器
 * 扫描 SDA=21, SCL=22 总线上的所有 I2C 设备并打印地址
 */

#include <Wire.h>

#define SDA_PIN 21
#define SCL_PIN 22

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }
  delay(1000);

  Serial.println("\n=== ESP32 I2C 扫描器 ===");
  Serial.print("SDA=GPIO"); Serial.print(SDA_PIN);
  Serial.print(" SCL=GPIO"); Serial.println(SCL_PIN);
  Serial.println("开始扫描...\n");

  Wire.begin(SDA_PIN, SCL_PIN);
}

void loop() {
  byte found = 0;

  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    byte error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("找到 I2C 设备，地址: 0x");
      if (addr < 16) Serial.print("0");
      Serial.print(addr, HEX);
      Serial.print(" (");
      Serial.print(addr);
      Serial.println(")");
      found++;
      delay(10);
    }
  }

  if (found == 0) {
    Serial.println("没有找到任何 I2C 设备！请检查接线。");
  } else {
    Serial.print("\n共找到 ");
    Serial.print(found);
    Serial.println(" 个设备");
  }

  Serial.println("--------------------");
  delay(3000);
}
