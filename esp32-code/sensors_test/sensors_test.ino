#define PIR_PIN 4

void setup() {
  Serial.begin(115200);
  pinMode(PIR_PIN, INPUT_PULLDOWN);
  Serial.println("=== HC-SR501 测试 ===");
  Serial.println("预热 30 秒...");
  delay(30000);
  Serial.println("开始！");
}

void loop() {
  int val = digitalRead(PIR_PIN);
  if (val == HIGH) {
    Serial.println("有人!!");
  } else {
    Serial.println("无人");
  }
  delay(500);
}
