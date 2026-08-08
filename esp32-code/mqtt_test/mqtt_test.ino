/*
 * ESP32 智能家居系统 - MQTT 数据上传
 * Week 2 Day 3（Day 1=WiFi, Day 2=注册EMQX, Day 3=真正发MQTT数据）
 *
 * 功能：
 *   1. 连接 WiFi（2.4GHz）
 *   2. 通过 TLS 加密连接 EMQX Cloud MQTT Broker
 *   3. 每 2 秒读取所有传感器，以 JSON 格式发布到 Topic: sensor/data
 *   4. OLED 实时显示 + 串口同步输出
 *   5. 接收 Web 面板指令独立控制继电器和 LED 灯（各3模式：手动开/手动关/自动）
 *
 * 硬件连接：
 *   DHT22 温湿度  → GPIO 5  (3.3V 供电)
 *   HC-SR501 PIR  → GPIO 4  (5V 供电)
 *   继电器模块     → GPIO 23 (5V 供电，高电平触发)
 *   LED 灯模块    → GPIO 2  (3.3V 供电，S 脚接 GPIO2)
 *   OLED SSD1306  → I2C (SDA=GPIO21, SCL=GPIO22)
 *   BH1750 光照   → I2C (和 OLED 共用)
 *
 * 库依赖（Arduino IDE 库管理器安装）:
 *   1. DHT sensor library        (by Adafruit)
 *   2. BH1750                    (by Christopher Laws)
 *   3. Adafruit SSD1306          (by Adafruit)
 *   4. Adafruit GFX Library      (by Adafruit)
 *   5. PubSubClient              (by Nick O'Leary)     ← 新装这个！
 */

// ============ 引入库 ============
#include <WiFi.h>               // ESP32 WiFi 连接库（自带）
#include <WiFiClientSecure.h>   // TLS 加密连接库（自带，用于 HTTPS 和 MQTTS）
#include <PubSubClient.h>       // MQTT 客户端库（需安装，负责收发 MQTT 消息）
#include <Wire.h>               // I2C 通信库（自带，OLED 和 BH1750 都用 I2C）
#include <Adafruit_GFX.h>       // OLED 绘图基础库（需安装，画文字/线条/方块）
#include <Adafruit_SSD1306.h>   // SSD1306 OLED 驱动库（需安装，控制 0.96寸 OLED）
#include <DHT.h>                // DHT 系列温湿度传感器驱动（需安装）
#include <BH1750.h>             // BH1750 光照传感器驱动（需安装）

// ============ WiFi 配置 ============
const char* WIFI_SSID     = "204";            // WiFi 名字（改成你自己的）
const char* WIFI_PASSWORD = "18581569078";    // WiFi 密码（改成你自己的）

// ============ MQTT 配置 ============
const char* MQTT_BROKER   = "b71f890f.ala.cn-shenzhen.emqxsl.cn";  // EMQX Cloud 服务器地址
const int   MQTT_PORT     = 8883;             // TLS 加密端口（不是普通的 1883）
const char* MQTT_USER     = "esp32";          // EMQX 后台创建的用户名
const char* MQTT_PASS     = "123456";         // 你设的密码
const char* MQTT_TOPIC_DATA = "sensor/data";     // 发布传感器数据到这个话题
const char* MQTT_TOPIC_CMD  = "sensor/command";  // 从这个话题接收控制指令

// ============ EMQX CA 证书（根证书，验证服务器身份用）============
// TLS 连接时，服务器会出示证书，ESP32 用这个 CA 证书来验证"对方确实是 EMQX Cloud"
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
#define DHTPIN    5        // DHT22 数据脚接 GPIO 5
#define DHTTYPE   DHT22    // 传感器型号 DHT22（不是 DHT11）
#define PIR_PIN   4        // HC-SR501 人体红外传感器信号脚接 GPIO 4
#define RELAY_PIN 23       // 继电器模块信号脚接 GPIO 23（高电平=开，低电平=关）
#define LED_PIN   2        // LED 模块 S 脚接 GPIO 2（VCC→3.3V, GND→GND）

// ============ OLED 配置 ============
#define SCREEN_WIDTH  128  // OLED 屏幕宽度 128 像素
#define SCREEN_HEIGHT 64   // OLED 屏幕高度 64 像素
#define OLED_ADDR     0x3C // OLED 的 I2C 地址（常见的 0x3C 或 0x3D）

// ============ NTP 时间服务器（TLS 证书验证需要正确时间）============
// TLS 握手时会检查证书有效期，ESP32 需要知道"现在几点"才能验证
const char* NTP_SERVER1 = "ntp.aliyun.com";      // 阿里云 NTP 服务器
const char* NTP_SERVER2 = "ntp1.aliyun.com";     // 备用 NTP 服务器
const long   GMT_OFFSET_SEC = 8 * 3600;          // 东八区，比 UTC 快 8 小时（8×3600 秒）
const int    DAYLIGHT_OFFSET = 0;                // 中国没有夏令时，设为 0

// ============ 对象创建（每个传感器/通信模块都需要一个对象）============
WiFiClientSecure espClient;    // 创建一个 TLS 加密网络连接对象
PubSubClient      mqtt(espClient);  // 创建 MQTT 客户端，底层用上面的 TLS 连接
DHT               dht(DHTPIN, DHTTYPE);     // 创建 DHT22 传感器对象
BH1750            lightMeter;   // 创建 BH1750 光照传感器对象
Adafruit_SSD1306  display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);  // 创建 OLED 显示对象

// ============ 全局变量 ============
unsigned long lastPublish = 0;          // 记录上次发送 MQTT 的时间（用 millis() 比较）
const long    PUBLISH_INTERVAL = 2000;  // 发送间隔 2000 毫秒 = 2 秒
int           lastPir = -1;             // 记住上次 PIR 状态（-1=还没读过，0=无人，1=有人）
int           remoteRelay = -1;         // 继电器控制模式：-1=自动, 0=手动关, 1=手动开
int           remoteLed   = -1;         // LED 控制模式：-1=自动, 0=手动关, 1=手动开

#define LED_LUX_THRESHOLD 50  // 光照低于 50 勒克斯算"天暗"，LED 自动模式会用到这个阈值

// ============ JSON 解析辅助函数 ============
// 作用：检查收到的 MQTT 消息里是否包含 "key":"value" 这个组合
// 为什么要写这个：Arduino 不自带 JSON 解析库，手动用 strstr 查找字符串更轻量
// 兼容两种格式：{"key":"val"} 和 {"key": "val"}（冒号后面有没有空格都能匹配）
bool jsonHas(const char* msg, const char* key, const char* val) {
  char p1[32], p2[32];  // 临时字符串缓冲区
  snprintf(p1, sizeof(p1), "\"%s\":\"%s\"", key, val);     // 拼出 "key":"val"（无空格）
  snprintf(p2, sizeof(p2), "\"%s\": \"%s\"", key, val);    // 拼出 "key": "val"（有空格）
  return strstr(msg, p1) != NULL || strstr(msg, p2) != NULL;  // 只要匹配到其中一个就返回 true
}

// ============ MQTT 回调函数：收到 Web 面板发来的控制指令时自动调用 ============
// topic:   发来消息的话题（"sensor/command"）
// payload: 消息内容（字节数组，比如 {"relay":"on"}）
// length:  消息长度
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // 把 byte 数组转成 C 字符串（末尾加 '\0' 结束符）
  char msg[100];  // 最多存 99 个字符
  unsigned int len = length < 99 ? length : 99;  // 防止超出缓冲区
  memcpy(msg, payload, len);  // 复制内容
  msg[len] = '\0';            // 加结束符

  Serial.print("[MQTT] 收到指令: ");  // 串口打印收到的消息
  Serial.println(msg);

  // ---- 继电器命令解析 ----
  // 检查消息里有没有 "relay":"on" → 手动开继电器
  if (jsonHas(msg, "relay", "on")) {
    remoteRelay = 1;                    // 标记为"手动开"模式
    digitalWrite(RELAY_PIN, HIGH);     // 给继电器引脚高电平 → 继电器闭合 → 电器通电
    Serial.println("  → 继电器: 手动开");
  }
  // 检查 "relay":"off" → 手动关继电器
  else if (jsonHas(msg, "relay", "off")) {
    remoteRelay = 0;                    // 标记为"手动关"模式
    digitalWrite(RELAY_PIN, LOW);       // 低电平 → 继电器断开 → 电器断电
    Serial.println("  → 继电器: 手动关");
  }
  // 检查 "relay":"auto" → 切回自动模式
  else if (jsonHas(msg, "relay", "auto")) {
    remoteRelay = -1;                   // 标记为"自动"模式（后面 loop 会根据 PIR 自动控制）
    Serial.println("  → 继电器: 切回自动模式");
  }

  // ---- LED 命令解析（和继电器完全独立，互不影响）----
  // 检查 "led":"on" → 手动开 LED
  if (jsonHas(msg, "led", "on")) {
    remoteLed = 1;                      // 标记为"手动开"模式
    digitalWrite(LED_PIN, HIGH);       // 高电平 → LED 亮
    Serial.println("  → LED: 手动开");
  }
  // 检查 "led":"off" → 手动关 LED
  else if (jsonHas(msg, "led", "off")) {
    remoteLed = 0;                      // 标记为"手动关"模式
    digitalWrite(LED_PIN, LOW);         // 低电平 → LED 灭
    Serial.println("  → LED: 手动关");
  }
  // 检查 "led":"auto" → 切回自动模式
  else if (jsonHas(msg, "led", "auto")) {
    remoteLed = -1;                     // 标记为"自动"模式（后面 loop 会根据 PIR + 光照自动控制）
    Serial.println("  → LED: 切回自动模式");
  }
}

// ============ 时间同步函数 ============
// 为什么要同步时间：TLS 握手时要验证证书有效期，ESP32 必须知道当前时间
// 没有时间 → 无法验证证书 → TLS 连接失败 → MQTT 连不上
void syncTime() {
  // 配置 NTP 服务器，ESP32 会向这些服务器请求当前时间
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET, NTP_SERVER1, NTP_SERVER2);
  Serial.print("同步时间中");

  int retry = 0;              // 重试计数器
  struct tm timeinfo;         // 时间结构体（年月日时分秒）
  // 循环等待时间同步成功，最多重试 20 次（每次等 1 秒）
  while (!getLocalTime(&timeinfo) && retry < 20) {
    Serial.print(".");       // 每秒打印一个点，表示在等待
    delay(1000);              // 等 1 秒
    retry++;                  // 重试次数 +1
  }

  // 检查是否同步成功
  if (retry >= 20) {
    Serial.println("\n时间同步失败！检查 WiFi 能否访问互联网");
  } else {
    Serial.println("\n时间同步成功");
    Serial.print("当前时间: ");
    Serial.println(&timeinfo, "%Y-%m-%d %H:%M:%S");  // 打印格式化时间
  }
}

// ============ MQTT 连接函数 ============
void connectMQTT() {
  // 循环尝试连接，直到成功为止
  while (!mqtt.connected()) {
    Serial.print("MQTT 连接中...");

    // 生成随机 Client ID（每次不同，避免和旧会话冲突）
    String clientId = "esp32-";                // 前缀
    clientId += String(random(0xffff), HEX);   // 加一个随机十六进制数

    // 尝试连接：传入 Client ID + 用户名 + 密码
    if (mqtt.connect(clientId.c_str(), MQTT_USER, MQTT_PASS)) {
      Serial.println(" 成功!");
      mqtt.subscribe(MQTT_TOPIC_CMD);          // 订阅控制指令话题
      Serial.print("  已订阅: ");
      Serial.println(MQTT_TOPIC_CMD);
    } else {
      // 连接失败，打印状态码（常见：-4=超时, -2=认证失败, -1=断开）
      Serial.print(" 失败, 状态码=");
      Serial.print(mqtt.state());
      Serial.println(" (5秒后重试)");
      delay(5000);  // 等 5 秒再重试
    }
  }
}

// ============ 发布传感器数据到 MQTT ============
// 把温度、湿度、光照、PIR、继电器、LED 状态打包成 JSON 发出去
void publishSensorData(float temp, float humi, float lux, int pir, int relay, int led) {
  // 手动拼接 JSON 字符串（不用 ArduinoJson 库，省内存）
  // snprintf 格式化：%.1f=保留1位小数, %.0f=不保留小数, %d=整数
  char msg[200];  // JSON 消息缓冲区
  snprintf(msg, sizeof(msg),
    "{\"temp\":%.1f,\"humi\":%.1f,\"lux\":%.0f,\"pir\":%d,\"relay\":%d,\"led\":%d}",
    temp, humi, lux, pir, relay, led);

  // 发布到 sensor/data 话题
  if (mqtt.publish(MQTT_TOPIC_DATA, msg)) {
    Serial.print("MQTT 已发送: ");
    Serial.println(msg);
  } else {
    Serial.println("MQTT 发送失败!");  // 可能是 MQTT 断开了
  }
}

// ============ OLED 显示函数 ============
// 在 0.96 寸 OLED 上显示所有传感器状态和控制状态
void showOLED(float temp, float humi, float lux, int pir, int relay, int led,
              bool wifiOk, bool mqttOk) {
  display.clearDisplay();         // 清空屏幕缓冲区
  display.setTextColor(SSD1306_WHITE);  // 白色文字（OLED 是黑底白字）
  display.setTextSize(1);        // 字号 1（最小，6×8 像素/字符）

  // ---- 第 1 行：WiFi 和 MQTT 连接状态 ----
  display.setCursor(0, 0);       // 光标移到左上角
  display.print(wifiOk ? "W:OK" : "W:--");     // WiFi 连上显示 OK，没连上显示 --
  display.setCursor(60, 0);      // 光标移到右边
  display.print(mqttOk ? "MQTT:OK" : "MQTT:--");  // MQTT 同理

  // 分割线（横线，把状态栏和传感器数据分开）
  display.drawLine(0, 11, 128, 11, SSD1306_WHITE);

  // ---- 第 2 行：温度和湿度 ----
  display.setCursor(0, 14);
  display.print("T:");           // 温度标签
  if (!isnan(temp)) {            // isnan 检查是不是无效值（DHT22 读取失败会返回 NaN）
    display.print(temp, 1);      // 显示温度，保留 1 位小数
    display.print("C");          // 摄氏度
  } else {
    display.print("--");        // 读取失败显示 --
  }

  display.setCursor(64, 14);    // 右半边显示湿度
  display.print("H:");
  if (!isnan(humi)) {
    display.print(humi, 0);      // 湿度，不保留小数
    display.print("%");
  } else {
    display.print("--");
  }

  // ---- 第 3 行：光照强度 ----
  display.setCursor(0, 28);
  display.print("Light: ");
  if (lux >= 0) {               // BH1750 读取失败会返回负数
    display.print(lux, 0);     // 光照值（勒克斯）
    display.print(" lx");
  } else {
    display.print("--");
  }

  // ---- 第 4 行：人体检测（PIR）----
  display.setCursor(0, 42);
  display.print("PIR: ");
  if (pir == HIGH) {
    // 有人时画一个反色高亮方块（白底黑字），醒目
    display.fillRect(35, 40, 48, 12, SSD1306_WHITE);  // 画白色方块
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE); // 设为黑字白底
    display.print("SOMEONE!");   // 显示"有人"
    display.setTextColor(SSD1306_WHITE);  // 恢复白字黑底
  } else {
    display.print("Empty");      // 无人
  }

  // ---- 第 5 行：继电器 + LED 状态（各自独立显示）----
  display.setCursor(0, 56);
  display.print("R:");                          // R = Relay 继电器
  display.print(relay == HIGH ? "ON " : "OFF");  // 高电平=ON，低电平=OFF
  display.setCursor(45, 56);
  display.print("L:");                           // L = LED 灯
  display.print(led == HIGH ? "ON " : "OFF");    // 同上
  if (pir == HIGH) {                             // 如果有人，最右边显示 PIR!
    display.setCursor(90, 56);
    display.print("PIR!");
  }

  display.display();  // 把缓冲区内容刷新到屏幕上（必须调用，否则不显示）
}

// ============ SETUP（开机只执行一次）============
void setup() {
  Serial.begin(115200);         // 串口波特率 115200（串口监视器也要设成这个值）
  randomSeed(analogRead(0));    // 用未接的模拟引脚噪声做随机种子（生成随机 Client ID 用）

  // ---- 引脚初始化 ----
  pinMode(PIR_PIN, INPUT_PULLDOWN);  // PIR 引脚设为输入，默认低电平（无人时稳定为 0）
  pinMode(RELAY_PIN, OUTPUT);        // 继电器引脚设为输出
  digitalWrite(RELAY_PIN, LOW);      // 开机默认继电器关闭
  pinMode(LED_PIN, OUTPUT);          // LED 引脚设为输出
  digitalWrite(LED_PIN, LOW);         // 开机默认 LED 关闭

  // ---- 传感器初始化 ----
  dht.begin();                  // 启动 DHT22 传感器
  Wire.begin(21, 22);           // 启动 I2C，SDA=GPIO21, SCL=GPIO22（OLED 和 BH1750 共用）

  // ---- OLED 初始化 ----
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED 失败");  // 初始化失败（检查接线）
  }
  // 开机画面
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(10, 20);
  display.println("MQTT System");    // 显示系统名称
  display.setCursor(10, 36);
  display.println("Connecting...");  // 显示正在连接
  display.display();                 // 刷新到屏幕

  // ---- BH1750 光照传感器初始化 ----
  if (!lightMeter.begin()) {
    Serial.println("BH1750 失败");  // 初始化失败（检查 I2C 接线）
  }

  // ---- 连接 WiFi ----
  Serial.print("连接 WiFi: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);  // 开始连接 WiFi

  // 等待 WiFi 连接，最多等 30 秒
  int wifiTry = 0;
  while (WiFi.status() != WL_CONNECTED && wifiTry < 30) {
    delay(1000);          // 每秒检查一次
    Serial.print(".");    // 打印点表示在等待
    wifiTry++;
  }

  // 检查 WiFi 是否连上
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi 已连接");
    Serial.print("IP: ");                    // 打印 ESP32 分到的 IP 地址
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi 连接失败!");
  }

  // ---- 同步时间（TLS 连接前必须做）----
  syncTime();

  // ---- MQTT TLS 配置 ----
  espClient.setCACert(CA_CERT);       // 加载 CA 证书（验证 EMQX Cloud 服务器身份）
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);  // 设置 MQTT 服务器地址和端口
  mqtt.setCallback(mqttCallback);     // 注册回调函数（收到消息时自动调用）
  connectMQTT();                     // 开始连接 MQTT

  // ---- HC-SR501 PIR 传感器预热（上电后需要约 10 秒稳定）----
  for (int i = 10; i > 0; i--) {      // 10 秒倒计时
    display.clearDisplay();
    display.setCursor(20, 24);
    display.print("PIR warmup: ");    // 显示预热倒计时
    display.print(i);
    display.print("s");
    display.display();
    delay(1000);                     // 每秒减 1
  }

  Serial.println("=== MQTT 系统就绪 ===");
  display.clearDisplay();
  display.setCursor(20, 24);
  display.println("MQTT Ready!");     // 显示就绪
  display.display();
  delay(1000);                       // 停留 1 秒让人看清
}

// ============ LOOP（开机后不断循环执行）============
void loop() {
  // ---- WiFi 断线重连 ----
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi 断开，正在重连...");
    WiFi.reconnect();               // 尝试重连 WiFi
    delay(5000);                    // 等 5 秒
    if (WiFi.status() == WL_CONNECTED) {
      syncTime();                   // WiFi 恢复后重新同步时间（断线期间时间可能偏差）
    }
  }

  // ---- MQTT 断线重连 ----
  if (!mqtt.connected()) {
    connectMQTT();                  // 尝试重新连接 MQTT
  }
  mqtt.loop();                      // 处理 MQTT 后台任务（必须每轮调用，负责心跳包、接收消息等）

  // ---- 读取所有传感器 ----
  float temp  = dht.readTemperature();   // 读温度（℃）
  float humi  = dht.readHumidity();      // 读湿度（%）
  float lux   = lightMeter.readLightLevel();  // 读光照（勒克斯 lx）
  int   pir   = digitalRead(PIR_PIN);     // 读人体红外（HIGH=有人, LOW=无人）

  // ---- 自动控制逻辑（继电器和LED各自独立）----

  // 继电器自动模式：有人 → 开继电器，无人 → 关继电器
  if (remoteRelay == -1) {                 // -1 表示自动模式
    if (pir == HIGH) {                     // PIR 检测到有人
      digitalWrite(RELAY_PIN, HIGH);      // 开继电器
    } else {                               // 无人
      digitalWrite(RELAY_PIN, LOW);       // 关继电器
    }
  }
  // 注意：如果 remoteRelay != -1（手动模式），这里什么都不做
  //       继电器保持 mqttCallback 里设置的状态不变

  // LED 自动模式：有人 并且 天暗 → 亮灯，否则 → 灭灯
  if (remoteLed == -1) {                                        // -1 表示自动模式
    if (pir == HIGH && lux < LED_LUX_THRESHOLD) {              // 有人 且 光照 < 50lx（天暗）
      digitalWrite(LED_PIN, HIGH);                             // 亮灯
    } else {                                                    // 没人 或 天不暗
      digitalWrite(LED_PIN, LOW);                              // 灭灯
    }
  }
  // 同理：remoteLed != -1（手动模式）时，LED 保持 mqttCallback 设的状态

  // 读取当前继电器和 LED 的实际状态（用于发送数据和显示）
  int relay = digitalRead(RELAY_PIN);   // 读继电器引脚当前电平
  int led   = digitalRead(LED_PIN);     // 读 LED 引脚当前电平

  // ---- 定时发布 MQTT 数据 ----
  unsigned long now = millis();                              // 获取开机后经过的毫秒数
  bool shouldPublish = (now - lastPublish >= PUBLISH_INTERVAL);  // 距上次发送是否够 2 秒

  // PIR 状态发生变化时 → 立刻发一条，不等 2 秒（保证事件实时性）
  if (lastPir != -1 && pir != lastPir) {                    // lastPir != -1 排除第一次读取
    Serial.print("⚠ PIR 变化: ");
    Serial.print(lastPir ? "有人→无人" : "无人→有人");
    shouldPublish = true;                                    // 标记为"需要发送"
  }
  lastPir = pir;  // 更新上次 PIR 状态，供下一轮比较

  // 如果需要发送数据
  if (shouldPublish) {
    lastPublish = now;  // 更新上次发送时间

    // 双重保险：发送前再检查一次 MQTT 连接
    if (!mqtt.connected()) {
      connectMQTT();    // 断了就重连
    }

    publishSensorData(temp, humi, lux, pir, relay, led);  // 发送 JSON 数据
  }

  // ---- OLED 显示 ----
  showOLED(temp, humi, lux, pir, relay, led,
           WiFi.status() == WL_CONNECTED,  // WiFi 是否连着
           mqtt.connected());               // MQTT 是否连着

  delay(500);  // 每轮循环等 500 毫秒（控制循环速度，不要太快）
}
