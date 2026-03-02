// NASPanel 固件示例：USB CDC 串口按行接收并回显（清爽注释版）
#include <Arduino.h> //                                                        Arduino 核心 API（Serial / delay 等）
#include <ArduinoJson.h> //                                                    JSON 解析（PlatformIO: lib_deps = bblanchon/ArduinoJson）
#include <string.h>

// 上位机每秒发送一行 JSON（NDJSON），这里保存解析后的字段（按需增减）
struct NasStats {
  char ts[20]; //                                                              "2026-03-01T22:09:39" (19) + '\0'
  char ip[16]; //                                                              IPv4 最大 15 + '\0'

  float cpu_usage;
  float cpu_temp_c;
  int mem_used_mb;
  int mem_total_mb;
  float load1;
  float load5;
  float load15;
  long uptime_s;
  long cpu_freq_mhz;
  float net_rx_kbs;
  float net_tx_kbs;
};

// 初始化 USB CDC 串口
static void initUsbSerial(uint32_t baud = 115200, bool waitForHost = true) {
  Serial.begin(baud); //                                                        启动串口（CDC 上该参数通常仅用于对齐上位机约定）
  Serial.setTimeout(5); //                                                      readBytesUntil 的最长阻塞时间（ms）

  // 可选：等待主机打开串口，避免丢首包
  if (waitForHost) {
    // 某些板子需要等待主机连接
    while (!Serial) {
      delay(10); //                                                            小延时避免忙等
    }
  }

  Serial.println("USB Serial Ready"); //                                        连接就绪提示
}

// 读取一行到缓冲区（以 '\n' 结束）
static bool receiveLineFromSerial(char *outBuf, size_t outCap, size_t &outLen) {
  outLen = 0; //                                                                默认输出长度为 0
  // 参数保护：至少留出 '\0'
  if (outBuf == nullptr || outCap < 2) return false;
  // 没有数据则直接返回
  if (!Serial.available()) return false;

  size_t n = Serial.readBytesUntil('\n', outBuf, outCap - 1); //                读取到 '\n' 或超时（不包含 '\n'）
  if (n == 0) return false; //                                                  超时/无数据

  // 行过长被截断：丢弃本行剩余内容直到 '\n'
  if (n >= outCap - 1) {
    // 继续读走剩余字节
    while (Serial.available() > 0) {
      if ((char)Serial.read() == '\n') break; //                                读到行尾则停止
    }
  }

  if (n > 0 && outBuf[n - 1] == '\r') n--; //                                   兼容 CRLF：去掉末尾 '\r'
  outBuf[n] = '\0'; //                                                          C 字符串结尾
  outLen = n; //                                                                回传有效长度（不含 '\0'）
  return true; //                                                               读取成功
}

// 解析一行 JSON 到结构体（方案C：输入为 char* + len，避免 String 碎片化）
static bool parseNasStatsJson(const char *line, size_t len, NasStats &out) {
  StaticJsonDocument<768> doc; //                                               够用就好，不够会反序列化失败

  DeserializationError err = deserializeJson(doc, line, len);
  if (err) return false;

  const char *ts = doc["ts"] | "";
  const char *ip = doc["ip"] | "";
  strlcpy(out.ts, ts, sizeof(out.ts));
  strlcpy(out.ip, ip, sizeof(out.ip));

  out.cpu_usage    = doc["cpu_usage"]    | 0.0f;
  out.cpu_temp_c   = doc["cpu_temp_c"]   | 0.0f;
  out.mem_used_mb  = doc["mem_used_mb"]  | 0;
  out.mem_total_mb = doc["mem_total_mb"] | 0;
  out.load1        = doc["load1"]        | 0.0f;
  out.load5        = doc["load5"]        | 0.0f;
  out.load15       = doc["load15"]       | 0.0f;
  out.uptime_s     = doc["uptime_s"]     | 0L;
  out.cpu_freq_mhz = doc["cpu_freq_mhz"] | 0L;
  out.net_rx_kbs   = doc["net_rx_kbs"]   | 0.0f;
  out.net_tx_kbs   = doc["net_tx_kbs"]   | 0.0f;

  return true;
}

// 输出回显
static void respondEchoToSerial(const char *line, size_t len) {
  Serial.print("RX: "); //                                                      前缀
  if (line != nullptr && len > 0) Serial.write((const uint8_t *)line, len); //  原样输出主体
  Serial.println(); //                                                         换行
  Serial.flush(); //                                                           等待 TX 刷出，减少后续输出被挤占的概率
}

void setup() { //                                                              上电/复位后执行一次
  initUsbSerial(115200, true); //                                               初始化串口并等待主机连接
}

void loop() { //                                                               主循环
  static char msg[768]; //                                                      接收缓冲区（存放一行）
  size_t msgLen = 0; //                                                         本次实际接收长度
  // 若收到一行，则回显给主机
  if (receiveLineFromSerial(msg, sizeof(msg), msgLen)) {
    respondEchoToSerial(msg, msgLen);

    NasStats s{};
    if (parseNasStatsJson(msg, msgLen, s)) {
      // 解析成功：打印一行摘要（后续你可以把这些值显示到屏幕）
      Serial.printf("OK ts=%s ip=%s cpu=%.1f%% temp=%.1fC mem=%d/%dMB net=%.2f/%.2fKiB/s\n",
                    s.ts, s.ip, s.cpu_usage, s.cpu_temp_c,
                    s.mem_used_mb, s.mem_total_mb,
                    s.net_rx_kbs, s.net_tx_kbs);
    } else {
      Serial.println("ERR: json");
    }
  }
}

