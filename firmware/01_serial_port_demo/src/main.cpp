#include <Arduino.h>

static constexpr uint8_t PIN_GPIO43 = 43;
static constexpr uint8_t PIN_GPIO44 = 44;

static unsigned long lastToggleMs = 0;
static bool level = false;
void setup() {
  // 启动 USB CDC 串口
  Serial.begin(115200);

  // 等待 USB 主机连接（可选，但推荐）
  while (!Serial) {
    delay(10);
  }

  Serial.println("USB Serial Ready");

  pinMode(PIN_GPIO43, OUTPUT);
  pinMode(PIN_GPIO44, OUTPUT);
  digitalWrite(PIN_GPIO43, LOW);
  digitalWrite(PIN_GPIO44, LOW);
}

void loop() {
  // 如果有数据
  if (Serial.available()) {
    // 读取一行（以换行符结尾）
    String msg = Serial.readStringUntil('\n');

    // 去掉多余空白
    msg.trim();

    // 打印收到的内容
    Serial.print("RX: ");
    Serial.println(msg);

    // 回显
    Serial.print("ECHO: ");
    Serial.println(msg);
  }

  const unsigned long now = millis();
  if (now - lastToggleMs >= 500) {
    lastToggleMs = now;
    level = !level;
    digitalWrite(PIN_GPIO43, level ? HIGH : LOW);
    digitalWrite(PIN_GPIO44, level ? HIGH : LOW);
  }

  delay(10); // 防止空转
}
