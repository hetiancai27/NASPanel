# linux_app：NAS 状态串口 JSON 发送器

本目录包含一个 Linux 小工具 `nas_serial_stats`（源码：`nas_stats_json_serial.c`），用于**周期性采集主机/NAS 的运行状态**，并将数据以**单行 JSON（NDJSON）**形式输出到：

- **stdout**（方便你在终端/管道里查看或转存）
- **串口**（115200 8N1，每条消息以 `\n` 结尾），发送给 ESP32 固件显示/处理

与仓库中的固件示例匹配：

- `firmware/01_serial_port_demo`：串口收一行（`\n` 结尾）并回显 `RX:`/`ECHO:`
- `firmware/03_naspanel`：同样使用 `Serial.readStringUntil('\n')` 接收一行

---

## 运行环境与依赖

- **操作系统**：Linux（依赖 `/proc`、`/sys`）
- **编译器**：`gcc`
- **命令依赖**：`ip`（来自 `iproute2`，用于获取本机 IPv4）
- **串口权限**：需要对 `/dev/ttyACM0` / `/dev/ttyUSB0` 等设备具备读写权限

> 注：CPU 温度与频率在部分设备/内核上可能取不到，程序会输出 `null`。

---

## 编译

在 `linux_app/` 目录下执行：

```bash
gcc -O2 -o nas_serial_stats nas_stats_json_serial.c
```

生成可执行文件 `nas_serial_stats`。

---

## 使用方法

程序用法（与源码头部说明一致）：

```bash
./nas_serial_stats [串口设备] [间隔秒数] [回显]
```

### 1) 默认用法（推荐先试）

默认串口：`/dev/ttyACM0`，默认间隔：1 秒，默认打印回显摘要：

```bash
./nas_serial_stats
```

### 2) 指定串口设备与间隔

例如每 2 秒发送一次：

```bash
./nas_serial_stats /dev/ttyACM0 2
```

常见设备名：

- **USB CDC**：`/dev/ttyACM0`
- **USB 转串口**：`/dev/ttyUSB0`

### 3) 关闭串口回显输出

只看本机发出的 JSON（stdout 仍会输出 JSON），不在终端打印 ESP32 回显：

```bash
./nas_serial_stats /dev/ttyACM0 1 0
```

第三个参数也支持 `q` / `quiet` 表示安静模式。

### 4) 仅 stdout（不打开串口）

适合调试采集逻辑或把 JSON 喂给别的程序：

```bash
./nas_serial_stats - 1
# 或
./nas_serial_stats stdout 1
```

---

## 输出 JSON 格式（每行一条）

每次发送/输出一行 JSON，字段如下（示例为示意）：

```json
{
  "ts":"2026-03-02T12:34:56",
  "cpu_usage":12.3,
  "cpu_temp_c":45.6,
  "mem_used_mb":1024,
  "mem_total_mb":8192,
  "load1":0.12,
  "load5":0.18,
  "load15":0.22,
  "ip":"192.168.1.10",
  "uptime_s":123456,
  "cpu_freq_mhz":1600,
  "net_rx_kbs":256.50,
  "net_tx_kbs":12.34
}
```

说明：

- **`cpu_temp_c`**：读取 `/sys/class/thermal/thermal_zone*/temp`，取不到则为 `null`
- **`cpu_freq_mhz`**：优先读 `scaling_cur_freq`，否则从 `/proc/cpuinfo` 平均，取不到则为 `null`
- **`net_rx_kbs` / `net_tx_kbs`**：基于 `/proc/net/dev` 的字节增量计算，单位 KiB/s（\(1024\) 字节）
- **`ip`**：通过 `ip -4 route get 1.1.1.1` 解析 `src` 字段（没有 `ip` 或无路由时可能保持 `0.0.0.0`）

---

## 与 ESP32 固件对接（串口协议）

- **串口参数**：115200、8N1
- **消息边界**：**每条消息以换行符 `\n` 结尾**
- **推荐接收方式**：`Serial.readStringUntil('\n')`

你可以直接刷入并测试 `firmware/01_serial_port_demo`：

- 主机端运行 `./nas_serial_stats /dev/ttyACM0 1`
- ESP32 会打印 `RX:` 与 `ECHO:`（本程序默认在终端显示摘要）

---

## 常见问题（FAQ）

### 1) `serial open failed` / 没有权限打开串口

把当前用户加入 `dialout`（不同发行版组名可能不同）并重新登录：

```bash
sudo usermod -aG dialout "$USER"
```

也可以临时改权限（重插或重启后可能失效）：

```bash
sudo chmod a+rw /dev/ttyACM0
```

### 2) 找不到设备 `/dev/ttyACM0`

确认设备名：

```bash
ls -l /dev/ttyACM* /dev/ttyUSB*
```

或查看内核日志：

```bash
dmesg | tail -n 50
```

### 3) `cpu_temp_c`/`cpu_freq_mhz` 为 `null`

这是正常降级行为：不同硬件/内核的温度与频率节点路径可能不存在或权限不足。

### 4) 想把输出交给别的程序（比如写入文件/通过管道处理）

本程序 stdout 永远输出 JSON（即使串口也打开），因此可直接重定向：

```bash
./nas_serial_stats stdout 1 >> stats.ndjson
```

---

## 目录内容

- `nas_stats_json_serial.c`：源码（采集 + stdout + 串口发送）
- `README.md`：本说明

