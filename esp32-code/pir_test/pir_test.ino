#define PIR_PIN 4

bool wasDetected = false;  // 记录之前的状态

void setup() {
  Serial.begin(115200);
  pinMode(PIR_PIN, INPUT_PULLDOWN);
  Serial.println("=== HC-SR501 测试 v2 ===");
  Serial.println("预热 30 秒...");
  delay(30000);
  Serial.println("开始！走到传感器前面挥手试试");
}

void loop() {
  int val = digitalRead(PIR_PIN);

  if (val == HIGH) {
    // 连续检测 5 次（2.5秒），排除瞬时抖动
    int count = 0;
    for (int i = 0; i < 5; i++) {
      if (digitalRead(PIR_PIN) == HIGH) count++;
      delay(500);
    }
    if (count >= 4 && !wasDetected) {
      Serial.println(">>> 有人进来了!! <<<");
      wasDetected = true;
    } else if (count >= 4) {
      Serial.println("有人(持续)");
    } else {
      // 不到4次就是误报，不输出
    }
  } else {
    if (wasDetected) {
      Serial.println("无人(人走了)");
      wasDetected = false;
    }
    delay(500);
  }
}
