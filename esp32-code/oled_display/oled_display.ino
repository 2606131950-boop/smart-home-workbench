/*
 * ESP32 智能家居系统 - 全传感器整合 + OLED 显示
 * Week 1 Day 8 最终实验
 *
 * 传感器/模块:
 *   DHT22    温湿度    OUT -> GPIO 5
 *   BH1750   光照度    I2C (SDA=21, SCL=22)
 *   HC-SR501 人体红外  OUT -> GPIO 4
 *   继电器   控制家电  IN  -> GPIO 23
 *   OLED     0.96寸    I2C (SDA=21, SCL=22)  与 BH1750 共用总线
 *
 * 库依赖（Arduino IDE 库管理器安装）:
 *   1. DHT sensor library        (by Adafruit)
 *   2. BH1750                    (by Christopher Laws)
 *   3. Adafruit SSD1306          (by Adafruit)
 *   4. Adafruit GFX Library      (by Adafruit)
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <BH1750.h>

// ============ 引脚定义 ============
#define DHTPIN    5       // DHT22 数据脚
#define DHTTYPE   DHT22   // 传感器型号
#define PIR_PIN   4       // HC-SR501 信号脚
#define RELAY_PIN 23      // 继电器控制脚

// ============ OLED 参数 ============
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_ADDR     0x3C   // 大多数 0.96寸 OLED 是 0x3C，不亮就改 0x3D

// ============ 对象创建 ============
DHT dht(DHTPIN, DHTTYPE);
BH1750 lightMeter;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void setup() {
  Serial.begin(115200);

  // --- 引脚初始化 ---
  pinMode(PIR_PIN, INPUT_PULLDOWN);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);  // 继电器初始关闭

  // --- I2C 初始化（SDA=21, SCL=22 是 ESP32 默认脚）---
  Wire.begin(21, 22);

  // --- OLED 初始化 ---
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED 初始化失败！检查接线或地址");
    while (true);  // 卡死
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(20, 0);
  display.println("Smart Home");
  display.setCursor(15, 16);
  display.println("Initializing...");
  display.display();

  // --- 传感器初始化 ---
  dht.begin();
  if (!lightMeter.begin()) {
    Serial.println("BH1750 初始化失败！检查 I2C 接线");
  }

  Serial.println("=== 全传感器 + OLED 整合测试 ===");

  // --- HC-SR501 预热 30 秒（OLED 倒计时显示）---
  for (int i = 30; i > 0; i--) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("HC-SR501");
    display.println("Preheating...");
    display.setTextSize(2);
    display.setCursor(45, 30);
    display.print(i);
    display.setTextSize(1);
    display.setCursor(30, 55);
    display.println("seconds");
    display.display();
    delay(1000);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(30, 28);
  display.println("Ready!");
  display.display();
  delay(1000);
}

void loop() {
  // === 读取所有传感器 ===
  float humidity = dht.readHumidity();       // 湿度 %
  float temp     = dht.readTemperature();    // 温度 C
  float lux      = lightMeter.readLightLevel(); // 光照 lx
  int   pir      = digitalRead(PIR_PIN);     // 人体 0/1
  // 有人自动开继电器，人走自动关
  if (pir == HIGH) {
    digitalWrite(RELAY_PIN, HIGH);
  } else {
    digitalWrite(RELAY_PIN, LOW);
  }
  int   relay    = digitalRead(RELAY_PIN);   // 继电器 0/1

  // === 串口输出 ===
  Serial.println("========================");
  Serial.print("温度:   "); Serial.print(isnan(temp) ? "--" : String(temp, 1)); Serial.println(" C");
  Serial.print("湿度:   "); Serial.print(isnan(humidity) ? "--" : String(humidity, 1)); Serial.println(" %");
  Serial.print("光照:   "); Serial.print(lux < 0 ? "--" : String(lux, 0)); Serial.println(" lx");
  Serial.print("人体:   "); Serial.println(pir == HIGH ? "有人" : "无人");
  Serial.print("继电器: "); Serial.println(relay == HIGH ? "ON" : "OFF");

  // === OLED 显示 ===
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  // 标题栏
  display.setCursor(0, 0);
  display.println("--- Smart Home ---");

  // 温度
  display.setCursor(0, 12);
  display.print("T:");
  if (!isnan(temp)) {
    display.print(temp, 1);
    display.print("C ");
  } else {
    display.print("--  ");
  }

  // 湿度（同一行右半）
  display.print("H:");
  if (!isnan(humidity)) {
    display.print(humidity, 0);
    display.print("%");
  } else {
    display.print("--");
  }

  // 光照
  display.setCursor(0, 26);
  display.print("Light: ");
  if (lux >= 0) {
    display.print(lux, 0);
    display.println(" lx");
  } else {
    display.println("--");
  }

  // 人体检测
  display.setCursor(0, 40);
  display.print("PIR: ");
  if (pir == HIGH) {
    // 有人时反白高亮
    display.fillRect(35, 38, 40, 11, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    display.print("SOMEONE!");
    display.setTextColor(SSD1306_WHITE);
  } else {
    display.print("Empty");
  }

  // 继电器状态
  display.setCursor(0, 54);
  display.print("Relay: ");
  display.println(relay == HIGH ? "ON " : "OFF");

  display.display();

  delay(2000);  // 2 秒刷新一次
}
