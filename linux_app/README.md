# linux_app 规范命令

本目录为 NAS 主机端 C 程序：采集系统状态、通过串口与 ESP32 通信。下文为**编译**与**运行**的规范命令。

---

## 一、程序与产物

| 源码 | 可执行文件 | 说明 |
|------|------------|------|
| `nas_stats_json_stdout.c` | `nas_usb_stats` | 仅向 stdout 周期性输出一行 JSON（CPU/内存/负载/温度/网速等） |
| `nas_stats_json_serial.c` | `nas_serial_stats` | 同上一套 JSON，同时写串口发往 ESP32，并可读串口回显 |
| `serial_send_demo.c` | `serial_send` | 示例：向串口每秒发一行 "Hello ESP32" 并打印回显 |

---

## 二、编译（规范命令）

建议在 **Debian Bullseye** 环境编译（如本机 glibc 与主机不一致，可用 Docker），保证与常见 NAS/CasaOS 兼容。

### 1. nas_usb_stats

```bash
cd /path/to/NASPanel/linux_app
gcc -O2 -o nas_usb_stats nas_stats_json_stdout.c
```

**Docker 方式（Bullseye）：**

```bash
cd /path/to/NASPanel/linux_app
docker run --rm -v "$(pwd)":/src -w /src debian:bullseye bash -c "apt-get update -qq && apt-get install -y -qq gcc libc6-dev && gcc -O2 -o nas_usb_stats nas_stats_json_stdout.c"
```

### 2. nas_serial_stats

```bash
cd /path/to/NASPanel/linux_app
gcc -O2 -o nas_serial_stats nas_stats_json_serial.c
```

**Docker 方式（Bullseye）：**

```bash
cd /path/to/NASPanel/linux_app
docker run --rm -v "$(pwd)":/src -w /src debian:bullseye bash -c "apt-get update -qq && apt-get install -y -qq gcc libc6-dev && gcc -O2 -o nas_serial_stats nas_stats_json_serial.c"
```

### 3. serial_send

```bash
cd /path/to/NASPanel/linux_app
gcc -O2 -o serial_send serial_send_demo.c
```

**Docker 方式：**

```bash
cd /path/to/NASPanel/linux_app
docker run --rm -v "$(pwd)":/src -w /src debian:bullseye bash -c "apt-get update -qq && apt-get install -y -qq gcc libc6-dev && gcc -O2 -o serial_send serial_send_demo.c"
```

---

## 三、运行（规范命令）

### 1. nas_usb_stats

- **含义**：按间隔秒数向 stdout 打一行 JSON，不涉及串口。
- **串口参数可省略**，仅用间隔。

```bash
# 每 1 秒一行（默认）
./nas_usb_stats

# 每 N 秒一行（兼容旧用法：第 1 个参数会被忽略）
./nas_usb_stats - 2
./nas_usb_stats x 5
```

### 2. nas_serial_stats

- **含义**：同上一套 JSON，同时写串口发往 ESP32；可选是否打印串口回显（摘要或关闭）。
- **串口默认** `/dev/ttyACM0`，115200 8N1，与 `firmware/01_serial_port_demo` 一致。

```bash
# 默认：/dev/ttyACM0，每 1 秒，回显摘要（[收到] RX/ECHO 字节数）
./nas_serial_stats

# 指定串口与间隔
./nas_serial_stats /dev/ttyACM0 2

# 不打印串口回显，终端只看到发出的 JSON
./nas_serial_stats /dev/ttyACM0 1 0

# 仅 stdout，不打开串口
./nas_serial_stats - 1
./nas_serial_stats stdout 1
```

### 3. serial_send

- **含义**：示例程序，向 `/dev/ttyACM0` 每秒发一行 "Hello ESP32 \<时间戳\>"，并打印串口回显。
- **设备写死在源码**，如需改设备请改源码后重新编译。

```bash
./serial_send
```

---

## 四、串口设备

- ESP32 USB CDC 常见节点：`/dev/ttyACM0`。
- 若为 USB 转串口芯片，可能为 `/dev/ttyUSB0`。
- 查看设备：`ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null`。

---

## 五、相关文档

- **nas_usb_stats 字段与原理**：见 `nas_usb_stats-参数说明.md`。
- **串口协议约定**：见项目内 `serial_protocol/README.md`（若存在）。
