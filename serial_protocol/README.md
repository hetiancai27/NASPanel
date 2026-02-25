# 串口协议说明

本目录说明 NAS 主机与 ESP32 单片机之间的串口通信格式。

## 连接与参数

- **串口**：主机 `/dev/ttyACM0`（或 `ttyUSB0`），ESP32 USB CDC
- **波特率**：115200，8N1
- **约定**：每行一条消息，以 `\n` 结尾

## 主机 → 单片机（下发的 JSON）

主机每秒发送一行 JSON，例如：

```json
{"ts":"2026-02-25T04:28:26","cpu_usage":17.7,"cpu_temp_c":41.0,"mem_used_mb":2038,"mem_total_mb":7801,"load1":0.26,"load5":0.32,"load15":0.27,"ip":"192.168.31.123","uptime_s":1641436,"cpu_freq_mhz":795,"net_rx_kbs":15.33,"net_tx_kbs":15.80}
```

| 字段 | 含义 |
|------|------|
| `ts` | 时间戳 ISO8601 |
| `cpu_usage` | CPU 使用率 % |
| `cpu_temp_c` | CPU 温度 ℃ |
| `mem_used_mb` / `mem_total_mb` | 内存 已用/总量 MB |
| `load1` / `load5` / `load15` | 系统负载 |
| `ip` | 本机 IP |
| `uptime_s` | 开机时长 秒 |
| `cpu_freq_mhz` | CPU 频率 MHz |
| `net_rx_kbs` / `net_tx_kbs` | 网速 收/发 KB/s |

## 单片机 → 主机（回显）

单片机收到一行后，会向串口回发两行（便于主机确认收到、并做简单回环测试）：

1. **RX: …** — 表示“我收到了这条”：
   ```
   RX: {"ts":"2026-02-25T04:28:26","cpu_usage":17.7,...}
   ```

2. **ECHO: …** — 即**单片机的回显**，把收到的同一条 JSON 原样发回：
   ```
   ECHO: {"ts":"2026-02-25T04:28:26","cpu_usage":17.7,...}
   ```

所以你在主机上看到的 **`[收到] ECHO: {"ts":...}` 就是单片机对下发的 JSON 的回显**（不是主机自己发的）。

## 相关代码

- 主机发送与采集：`linux_app/nas_serial_stats.c`
- 单片机收发与回显：`firmware/01_serial_port_demo/src/main.cpp`
