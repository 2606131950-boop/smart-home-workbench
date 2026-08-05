/*
 * ESP32 智能家居系统 - MQTT 数据上传
 * Week 2 Day 3（Day 1=WiFi, Day 2=注册EMQX, Day 3=真正发MQTT数据）
 *
 * 功能：
 *   1. 连接 WiFi（2.4GHz）
 *   2. 通过 TLS 加密连接 EMQX Cloud MQTT Broker
 *   3. 每 5 秒读取所有传感器，以 JSON 格式发布到 Topic: sensor/data
 *   4. OLED 实时显示 + 串口同步输出
 *
 * 硬件连接：
 *   见 final_pinout.md 或 MEMORY.md
 *
 * 库依赖（Arduino IDE 库管理器安装）:
 *   1. DHT sensor library        (by Adafruit)
 *   2. BH1750                    (by Christopher Laws)
 *   3. Adafruit SSD1306          (by Adafruit)
 *   4. Adafruit GFX Library      (by Adafruit)
 *   5. PubSubClient              (by Nick O'Leary)     ← 新装这个！
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <BH1750.h>

// ============ WiFi 配置 ============
const char* WIFI_SSID     = "204";       // ← 改成你的
const char* WIFI_PASSWORD = "18581569078";     // ← 改成你的

// ============ MQTT 配置 ============
const char* MQTT_BROKER   = "b71f890f.ala.cn-shenzhen.emqxsl.cn";
const int   MQTT_PORT     = 8883;               // TLS 加密端口
const char* MQTT_USER     = "esp32";            // ← 你在 EMQX 后台创建的用户名
const char* MQTT_PASS     = "123456";           // ← 你设的密码
const char* MQTT_TOPIC    = "sensor/data";       // 发布数据的话题

// ============ EMQX CA 证书（根证书，验证服务器身份用）============
const char* CA_CERT = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH
MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT
MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j
b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI
2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx
1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ
q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz
tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ
vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP
BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV
5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY
1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4
NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG
Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91
8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe
pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl
MrY=
-----END CERTIFICATE-----
)EOF";

// ============ 引脚定义 ============
#define DHTPIN    5
#define DHTTYPE   DHT22
#define PIR_PIN   4
#define RELAY_PIN 23

// ============ OLED ============
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_ADDR     0x3C

// ============ NTP 时间服务器（TLS 证书验证需要正确时间）============
const char* NTP_SERVER1 = "ntp.aliyun.com";
const char* NTP_SERVER2 = "ntp1.aliyun.com";
const long   GMT_OFFSET_SEC = 8 * 3600;   // 北京时间 UTC+8
const int    DAYLIGHT_OFFSET = 0;

// ============ 对象创建 ============
WiFiClientSecure espClient;
PubSubClient      mqtt(espClient);
DHT               dht(DHTPIN, DHTTYPE);
BH1750            lightMeter;
Adafruit_SSD1306  display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

unsigned long lastPublish = 0;
const long    PUBLISH_INTERVAL = 5000;  // 5 秒发一次

// ============ 时间同步（TLS 必需）============
void syncTime() {
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET, NTP_SERVER1, NTP_SERVER2);
  Serial.print("同步时间中");

  int retry = 0;
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo) && retry < 20) {
    Serial.print(".");
    delay(1000);
    retry++;
  }

  if (retry >= 20) {
    Serial.println("\n时间同步失败！检查 WiFi 能否访问互联网");
  } else {
    Serial.println("\n时间同步成功");
    Serial.print("当前时间: ");
    Serial.println(&timeinfo, "%Y-%m-%d %H:%M:%S");
  }
}

// ============ MQTT 连接 ============
void connectMQTT() {
  while (!mqtt.connected()) {
    Serial.print("MQTT 连接中...");

    // 生成唯一 Client ID
    String clientId = "esp32-";
    clientId += String(random(0xffff), HEX);

    if (mqtt.connect(clientId.c_str(), MQTT_USER, MQTT_PASS)) {
      Serial.println(" 成功!");
    } else {
      Serial.print(" 失败, 状态码=");
      Serial.print(mqtt.state());
      Serial.println(" (5秒后重试)");
      delay(5000);
    }
  }
}

// ============ 发布传感器数据（JSON 格式）============
void publishSensorData(float temp, float humi, float lux, int pir, int relay) {
  // 构造 JSON 消息（手动拼接，不需要 ArduinoJson 库）
  char msg[200];
  snprintf(msg, sizeof(msg),
    "{\"temp\":%.1f,\"humi\":%.1f,\"lux\":%.0f,\"pir\":%d,\"relay\":%d}",
    temp, humi, lux, pir, relay);

  if (mqtt.publish(MQTT_TOPIC, msg)) {
    Serial.print("MQTT 已发送: ");
    Serial.println(msg);
  } else {
    Serial.println("MQTT 发送失败!");
  }
}

// ============ OLED 显示 ============
void showOLED(float temp, float humi, float lux, int pir, int relay,
              bool wifiOk, bool mqttOk) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  // 第 1 行: 状态栏
  display.setCursor(0, 0);
  display.print(wifiOk ? "W:OK" : "W:--");
  display.setCursor(60, 0);
  display.print(mqttOk ? "MQTT:OK" : "MQTT:--");

  // 分割线
  display.drawLine(0, 11, 128, 11, SSD1306_WHITE);

  // 第 2 行: 温湿度
  display.setCursor(0, 14);
  display.print("T:");
  if (!isnan(temp)) { display.print(temp, 1); display.print("C"); }
  else { display.print("--"); }

  display.setCursor(64, 14);
  display.print("H:");
  if (!isnan(humi)) { display.print(humi, 0); display.print("%"); }
  else { display.print("--"); }

  // 第 3 行: 光照
  display.setCursor(0, 28);
  display.print("Light: ");
  if (lux >= 0) { display.print(lux, 0); display.print(" lx"); }
  else { display.print("--"); }

  // 第 4 行: PIR + 人体
  display.setCursor(0, 42);
  display.print("PIR: ");
  if (pir == HIGH) {
    display.fillRect(35, 40, 48, 12, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    display.print("SOMEONE!");
    display.setTextColor(SSD1306_WHITE);
  } else {
    display.print("Empty");
  }

  // 第 5 行: 继电器
  display.setCursor(0, 56);
  display.print("Relay: ");
  display.print(relay == HIGH ? "ON " : "OFF");

  display.display();
}

// ============ SETUP ============
void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(0));  // 用于生成随机 Client ID

  // 引脚
  pinMode(PIR_PIN, INPUT_PULLDOWN);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  // 传感器
  dht.begin();
  Wire.begin(21, 22);

  // OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED 失败");
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(10, 20);
  display.println("MQTT System");
  display.setCursor(10, 36);
  display.println("Connecting...");
  display.display();

  // BH1750
  if (!lightMeter.begin()) {
    Serial.println("BH1750 失败");
  }

  // WiFi
  Serial.print("连接 WiFi: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int wifiTry = 0;
  while (WiFi.status() != WL_CONNECTED && wifiTry < 30) {
    delay(1000);
    Serial.print(".");
    wifiTry++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi 已连接");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi 连接失败!");
  }

  // 同步时间（TLS 需要）
  syncTime();

  // MQTT TLS 配置
  espClient.setCACert(CA_CERT);  // 加载 CA 证书
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  connectMQTT();

  // HC-SR501 预热
  for (int i = 10; i > 0; i--) {
    display.clearDisplay();
    display.setCursor(20, 24);
    display.print("PIR warmup: ");
    display.print(i);
    display.print("s");
    display.display();
    delay(1000);
  }

  Serial.println("=== MQTT 系统就绪 ===");
  display.clearDisplay();
  display.setCursor(20, 24);
  display.println("MQTT Ready!");
  display.display();
  delay(1000);
}

// ============ LOOP ============
void loop() {
  // WiFi 重连
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi 断开，正在重连...");
    WiFi.reconnect();
    delay(5000);
    if (WiFi.status() == WL_CONNECTED) {
      syncTime();  // 时间可能偏差了
    }
  }

  // MQTT 重连
  if (!mqtt.connected()) {
    connectMQTT();
  }
  mqtt.loop();  // 处理 MQTT 后台任务（心跳包等）

  // ---- 读取传感器 ----
  float temp  = dht.readTemperature();
  float humi  = dht.readHumidity();
  float lux   = lightMeter.readLightLevel();
  int   pir   = digitalRead(PIR_PIN);

  // 自动控制：有人 → 开继电器
  if (pir == HIGH) {
    digitalWrite(RELAY_PIN, HIGH);
  } else {
    digitalWrite(RELAY_PIN, LOW);
  }
  int relay = digitalRead(RELAY_PIN);

  // ---- 定时发布 MQTT ----
  unsigned long now = millis();
  if (now - lastPublish >= PUBLISH_INTERVAL) {
    lastPublish = now;

    // MQTT 重连（双重保险）
    if (!mqtt.connected()) {
      connectMQTT();
    }

    publishSensorData(temp, humi, lux, pir, relay);
  }

  // ---- OLED 显示 ----
  showOLED(temp, humi, lux, pir, relay,
           WiFi.status() == WL_CONNECTED,
           mqtt.connected());

  delay(500);
}
