// 本文件：NASPanel 固件示例主程序入口与循环逻辑
#include <Arduino.h> // 引入 Arduino 核心 API（Serial / pinMode / millis 等）

// --- 函数化：初始化 / 接收 / 返回(响应) ---

static void initUsbSerial(uint32_t baud = 115200, bool waitForHost = true) {
  // 启动 USB CDC 串口（与 nas_serial_stats 一致：115200）
  Serial.begin(baud); // 初始化 USB 串口波特率（CDC 实际会忽略但保持一致）

  if (waitForHost) {
    // 等待 USB 主机连接（可选，但推荐）
    while (!Serial) { // 等待主机打开串口（某些板子需要）
      delay(10); // 小延时避免忙等占用过高
    } // 主机已连接/串口就绪
  }

  Serial.println("USB Serial Ready"); // 打印启动提示，方便确认串口已可用
}

// 接收函数：读取一行（以 '\n' 结束），并做 trim
static bool receiveLineFromSerial(String &outLine) {
  if (!Serial.available()) {
    return false;
  }

  outLine = Serial.readStringUntil('\n'); // 读取到换行符为止的一行文本（不包含 '\n'）
  outLine.trim(); // 去除首尾空白与换行
  return true;
}

// 返回函数：把收到的数据按约定回显/响应给主机
static void respondEchoToSerial(const String &line) {
  Serial.print("RX: "); // 输出接收前缀
  Serial.write(line.c_str(), line.length()); // 原样输出消息主体
  Serial.println(); // 输出换行结束本行
  Serial.flush(); // 避免 TX 缓冲导致回显丢字
}





void setup() { // 上电/复位后只执行一次：初始化
  initUsbSerial(115200, true);
}

void loop() { // 主循环：持续接收串口并返回
  String msg;
  if (receiveLineFromSerial(msg)) {
    respondEchoToSerial(msg);
  }
}

