/*
 * ESP32 智能家居系统 - WiFi 连接测试
 * Week 2 Day 1
 *
 * 功能：
 *   ESP32 连接 WiFi，OLED 显示连接状态和 IP 地址
 *
 * 接线：
 *   OLED 4针 I2C 接口（和 Week 1 一样，不用动线）
 *   GND -> GND
 *   VCC -> 3.3V
 *   SCL -> GPIO 22
 *   SDA -> GPIO 21
 *
 * 库依赖：
 *   Adafruit SSD1306 + Adafruit GFX Library（Week 1 已装）
 *   WiFi.h（ESP32 自带，不用装）
 */

#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ============ WiFi 配置（改成你自己的！！！）============
const char* WIFI_SSID     = "204";     // ← 改成你的 WiFi 名称
const char* WIFI_PASSWORD = "18581569078";    // ← 改成你的 WiFi 密码
// =========================================================

// ============ OLED 参数 ============
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_ADDR     0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void setup() {
  Serial.begin(115200);
  delay(100);

  // --- I2C + OLED 初始化 ---
  Wire.begin(21, 22);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED 初始化失败！检查接线");
    while (true);
  }

  // --- OLED 显示 "正在连接 WiFi" ---
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Connecting WiFi...");
  display.setCursor(0, 16);
  display.print("SSID: ");
  display.println(WIFI_SSID);
  display.display();

  // --- 开始连接 WiFi ---
  Serial.println("============================");
  Serial.print("正在连接 WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);           // 设为站点模式（连别人 WiFi）
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // --- 等待连接，最多等 20 秒 ---
  int dots = 0;
  while (WiFi.status() != WL_CONNECTED && dots < 20) {
    delay(1000);
    Serial.print(".");
    dots++;

    // OLED 上显示进度点
    display.setCursor(dots * 6, 32);
    display.print(".");
    display.display();
  }

  // --- 判断连接结果 ---
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("WiFi 连接成功！");
    Serial.print("IP 地址: ");
    Serial.println(WiFi.localIP());
    Serial.print("信号强度: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");

    // OLED 显示成功信息
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("WiFi Connected!");
    display.setCursor(0, 14);
    display.print("IP:");
    display.println(WiFi.localIP());
    display.setCursor(0, 28);
    display.print("RSSI: ");
    display.print(WiFi.RSSI());
    display.println(" dBm");
    display.setCursor(0, 42);
    display.print("SSID:");
    display.println(WIFI_SSID);
    display.setCursor(0, 56);
    display.println("Week2 Day1 OK!");
    display.display();

  } else {
    Serial.println();
    Serial.println("WiFi 连接失败！请检查：");
    Serial.println("1. WiFi 名和密码是否正确");
    Serial.println("2. WiFi 是否是 2.4GHz（ESP32 不支持 5GHz）");

    // OLED 显示错误
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("WiFi FAILED!");
    display.setCursor(0, 16);
    display.println("Check:");
    display.setCursor(0, 30);
    display.println("1. SSID/PWD correct?");
    display.setCursor(0, 44);
    display.println("2. Use 2.4GHz WiFi");
    display.setCursor(0, 56);
    display.println("(not 5GHz!)");
    display.display();
  }
}

void loop() {
  // 每 5 秒打印一次 WiFi 状态
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("[OK] IP: ");
    Serial.print(WiFi.localIP());
    Serial.print("  RSSI: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else {
    Serial.println("[LOST] WiFi 断开，尝试重连...");
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    delay(5000);
    return;
  }
  delay(5000);
}
