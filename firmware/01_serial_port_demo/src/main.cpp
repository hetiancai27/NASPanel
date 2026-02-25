/*
 * 01_serial_port_demo - USB 串口收发示例
 *
 * 与 linux_app/nas_serial_stats 匹配：115200 8N1，主机每行发一条消息（如 NAS 状态 JSON），
 * 以 \\n 结尾；本机用 readStringUntil('\\n') 接收后回显 "RX:" / "ECHO:"。
 * 可扩展为解析 JSON 后驱动屏幕或 LED。
 */
#include <Arduino.h>

static constexpr uint8_t PIN_GPIO43 = 43;
static constexpr uint8_t PIN_GPIO44 = 44;

static unsigned long lastToggleMs = 0;
static bool level = false;
void setup() {
  // 启动 USB CDC 串口（与 nas_serial_stats 一致：115200）
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
  // 接收主机发来的一行（如 nas_serial_stats 的 JSON），以 \n 结尾
  if (Serial.available()) {
    String msg = Serial.readStringUntil('\n');

    // 去掉多余空白
    msg.trim();

    // 打印收到的内容（长行用 write 避免 String 再分配）
    Serial.print("RX: ");
    Serial.write(msg.c_str(), msg.length());
    Serial.println();
    Serial.flush();  // 发完再发 ECHO，避免 TX 缓冲溢出导致 ECHO 丢字

    // 回显
    Serial.print("ECHO: ");
    Serial.write(msg.c_str(), msg.length());
    Serial.println();
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
