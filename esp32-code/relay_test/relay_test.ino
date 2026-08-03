#define RELAY_PIN 23

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);  // 初始关闭

  Serial.println("=== 继电器 + LED 测试 ===");
  Serial.println("继电器会在开关之间循环，听声音！");
}

void loop() {
  // 继电器吸合（LED 亮）
  digitalWrite(RELAY_PIN, HIGH);
  Serial.println("继电器 ON  | LED 亮  (会听到'咔'一声)");
  delay(2000);

  // 继电器断开（LED 灭）
  digitalWrite(RELAY_PIN, LOW);
  Serial.println("继电器 OFF | LED 灭");
  delay(2000);
}
