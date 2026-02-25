# nas_usb_stats 参数说明与获取原理

`nas_usb_stats` 周期性向 stdout 输出一行 JSON，包含系统运行状态。本文档说明各字段含义、数据来源及计算方式。

---

## 输出格式

每行一条 JSON，例如：

```json
{"ts":"2025-02-24T22:30:00","cpu_usage":12.5,"cpu_temp_c":45.2,"mem_used_mb":512,"mem_total_mb":2048,"load1":0.50,"load5":0.48,"load15":0.45,"ip":"192.168.1.100","uptime_s":86400,"cpu_freq_mhz":1200}
```

---

## 参数一览表

| JSON 字段        | 类型   | 含义           | 数据来源                         |
|------------------|--------|----------------|----------------------------------|
| `ts`             | string | 本地时间戳     | 系统时间 `time()` + `strftime`   |
| `cpu_usage`      | number | CPU 使用率 %   | `/proc/stat` 差值计算            |
| `cpu_temp_c`     | number/null | CPU 温度 °C | `/sys/class/thermal/thermal_zone*/temp` |
| `mem_used_mb`    | number | 已用内存 MB    | `/proc/meminfo`                  |
| `mem_total_mb`   | number | 总内存 MB      | `/proc/meminfo`                  |
| `load1`          | number | 1 分钟负载     | `/proc/loadavg`                  |
| `load5`          | number | 5 分钟负载     | `/proc/loadavg`                  |
| `load15`         | number | 15 分钟负载    | `/proc/loadavg`                  |
| `ip`             | string | 出网 IP        | `ip -4 route get 1.1.1.1` 中 src |
| `uptime_s`       | number | 运行时间 秒    | `/proc/uptime`                   |
| `cpu_freq_mhz`   | number/null | 当前 CPU 频率 MHz | `/sys/.../cpufreq` 或 `/proc/cpuinfo` |

---

## 各参数获取方式与原理

### 1. `ts`（时间戳）

- **来源**：本机系统时间。
- **方式**：`time(NULL)` 得到秒数，`localtime_r()` 转成本地时间，`strftime(..., "%Y-%m-%dT%H:%M:%S", ...)` 格式化为 ISO 风格字符串。
- **说明**：表示本条数据采集时刻的本地时间。

---

### 2. `cpu_usage`（CPU 使用率 %）

- **来源**：`/proc/stat` 第一行 `cpu` 汇总。
- **格式**：`cpu user nice system idle iowait irq softirq steal`（单位：内核 tick，一般 1 tick = 1/Hz 秒）。
- **原理**：
  - 每次读取 `idle`（程序中用 `idle + iowait`）和 `total`（上述 8 项之和）。
  - 与上一次采样做差值：`di = idle_now - idle_prev`，`dt = total_now - total_prev`。
  - 使用率 = `(1 - di/dt) * 100`，即“非空闲时间占比”的百分比。
- **周期**：与程序 `interval` 一致（默认 1 秒），间隔内平均使用率。

---

### 3. `cpu_temp_c`（CPU 温度 °C）

- **来源**：`/sys/class/thermal/thermal_zone<N>/temp`，N=0..15，取**第一个可读**的 zone。
- **文件内容**：整数，单位一般为**毫摄氏度**（如 45200 表示 45.2°C）。
- **处理**：若数值 > 1000 则除以 1000 得到 °C，否则直接当 °C；读不到或失败则为 `null`。
- **说明**：不同机器 thermal_zone 对应传感器可能不同（CPU/GPU/板载等），通常 zone0 多为 CPU 或 SoC。

---

### 4. `mem_used_mb` / `mem_total_mb`（内存 MB）

- **来源**：`/proc/meminfo`。
- **用到的项**：
  - `MemTotal:`：总内存（KB）。
  - `MemAvailable:`：可用内存（KB，约等于“空闲 + 可回收”）。
- **计算**：
  - `mem_total_mb = MemTotal / 1024`。
  - `mem_used_mb = (MemTotal - MemAvailable) / 1024`。
- **说明**：以“总 - 可用”为已用，比仅用 `MemFree` 更贴近实际使用（含 cache 可回收部分）。

---

### 5. `load1` / `load5` / `load15`（系统负载）

- **来源**：`/proc/loadavg`。
- **格式**：`load1 load5 load15 ...`（前三个浮点数）。
- **含义**：过去 1、5、15 分钟内，处于可运行状态（正在跑或等 CPU）的平均进程数（含线程），与 `uptime` 显示一致。
- **说明**：不区分 CPU 颗数，多核机器上负载 1.0 表示约满 1 核。

---

### 6. `ip`（本机出网 IP）

- **来源**：执行命令 `ip -4 route get 1.1.1.1`，从输出中解析 **`src`** 字段。
- **原理**：“到 1.1.1.1 的路由”会经过本机某块网卡，内核会填上该路由的源 IP，即本机对外的 IPv4 地址。
- **失败时**：若命令失败或解析不到 `src`，则输出 `"0.0.0.0"`。
- **依赖**：需要系统有 `ip` 命令（iproute2）。

---

### 7. `uptime_s`（运行时间 秒）

- **来源**：`/proc/uptime`。
- **格式**：第一列为系统自启动以来的秒数（浮点）。
- **使用**：读取该值取整得到 `uptime_s`。

---

### 8. `cpu_freq_mhz`（当前 CPU 频率 MHz）

- **优先来源**：`/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq`。
  - 内容为当前频率（kHz），除以 1000 得到 MHz。
- **备用来源**：若无 cpufreq 或读失败，则解析 `/proc/cpuinfo` 中所有 `cpu MHz` 行，**取平均**后作为当前频率（多核时为平均值）。
- **读不到**：输出 `null`。

---

## 运行方式

```bash
./nas_usb_stats [dev] [interval]
```

- `dev`：保留参数，当前未使用（可忽略）。
- `interval`：采样间隔（秒），默认 1。程序每 `interval` 秒输出一行 JSON 到 stdout。

---

## 依赖与权限

- **仅读**：`/proc/*`、`/sys/*` 为只读接口，通常无需 root。
- **命令**：获取 `ip` 时会执行 `ip -4 route get 1.1.1.1`，需系统已安装 iproute2 且可执行。
- **温度/频率**：部分嵌入式或容器环境可能没有 thermal 或 cpufreq，对应字段为 `null` 属正常。
