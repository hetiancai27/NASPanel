/*
 * nas_stats_json_serial.c - 系统状态采集 + 串口发送（每行一条 JSON）
 *
 * 周期性采集 CPU/内存/负载/温度/网速等，格式化为一行 JSON，通过串口发往 ESP32，
 * 同时输出到 stdout；并读取串口回显打印到终端（可选）。
 *
 * 与 firmware/01_serial_port_demo 匹配：
 *   - 波特率 115200 8N1，每行一条消息以 \n 结尾；
 *   - ESP32 使用 Serial.readStringUntil('\n') 接收，回显 "RX: ..." 与 "ECHO: ..."。
 *
 * 用法：./nas_serial_stats [串口设备] [间隔秒数] [回显]
 *   默认：./nas_serial_stats  =>  /dev/ttyACM0，每 1 秒，打印串口回显
 *   例：./nas_serial_stats /dev/ttyACM0 2
 *   不打印串口回显（仅看发出的 JSON）：./nas_serial_stats /dev/ttyACM0 1 0
 *   仅 stdout（不打开串口）：./nas_serial_stats - 1  或  ./nas_serial_stats stdout 1
 *
 * 编译：gcc -O2 -o nas_serial_stats nas_stats_json_serial.c
 */
 
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/select.h>
 
/* ---------- 状态采集（同 nas_stats_json_stdout.c） ---------- */
static int read_cpu_idle_total(unsigned long long *idle, unsigned long long *total) {
    FILE *f = fopen("/proc/stat", "r");
    if (!f) return -1;
    unsigned long long user,nice,sys,idle_v,iowait,irq,softirq,steal;
    int rc = fscanf(f, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
                    &user,&nice,&sys,&idle_v,&iowait,&irq,&softirq,&steal);
    fclose(f);
    if (rc < 8) return -1;
    *idle  = idle_v + iowait;
    *total = user + nice + sys + idle_v + iowait + irq + softirq + steal;
    return 0;
}
 
static int read_load(double *l1, double *l5, double *l15) {
    FILE *f = fopen("/proc/loadavg", "r");
    if (!f) return -1;
    int rc = fscanf(f, "%lf %lf %lf", l1, l5, l15);
    fclose(f);
    return (rc == 3) ? 0 : -1;
}
 
static int read_mem_mb(int *used_mb, int *total_mb) {
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return -1;
    long mt=0, ma=0;
    char key[64], unit[32];
    long val;
    while (fscanf(f, "%63s %ld %31s", key, &val, unit) == 3) {
        if (strcmp(key, "MemTotal:") == 0) mt = val;
        else if (strcmp(key, "MemAvailable:") == 0) ma = val;
        if (mt && ma) break;
    }
    fclose(f);
    if (!mt || !ma) return -1;
    *used_mb = (int)((mt - ma) / 1024);
    *total_mb = (int)(mt / 1024);
    return 0;
}
 
static double read_cpu_temp_c(void) {
    char path[128];
    for (int i = 0; i < 16; i++) {
        snprintf(path, sizeof(path), "/sys/class/thermal/thermal_zone%d/temp", i);
        FILE *f = fopen(path, "r");
        if (!f) continue;
        long v = 0;
        int rc = fscanf(f, "%ld", &v);
        fclose(f);
        if (rc == 1) {
            if (v > 1000) return (double)v / 1000.0;
            return (double)v;
        }
    }
    return -1.0;
}
 
static long read_uptime_s(void) {
    FILE *f = fopen("/proc/uptime", "r");
    if (!f) return 0;
    double up = 0;
    fscanf(f, "%lf", &up);
    fclose(f);
    return (long)up;
}
 
static long read_cpu_freq_mhz(void) {
    FILE *f = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", "r");
    if (f) {
        long khz = 0;
        if (fscanf(f, "%ld", &khz) == 1 && khz > 0) { fclose(f); return khz / 1000; }
        fclose(f);
    }
    f = fopen("/proc/cpuinfo", "r");
    if (!f) return -1;
    char *line = NULL;
    size_t n = 0;
    double sum = 0;
    int cnt = 0;
    while (getline(&line, &n, f) > 0) {
        if (strncasecmp(line, "cpu MHz", 7) == 0) {
            char *p = strchr(line, ':');
            if (p) { sum += atof(p + 1); cnt++; }
        }
    }
    free(line);
    fclose(f);
    return (cnt > 0) ? (long)(sum / cnt) : -1;
}
 
static int read_net_bytes(unsigned long long *rx_total, unsigned long long *tx_total) {
    FILE *f = fopen("/proc/net/dev", "r");
    if (!f) return -1;
    char line[512];
    *rx_total = 0;
    *tx_total = 0;
    if (!fgets(line, sizeof(line), f) || !fgets(line, sizeof(line), f)) {
        fclose(f);
        return -1;
    }
    while (fgets(line, sizeof(line), f)) {
        unsigned long long rx = 0, tx = 0;
        char *p = strchr(line, ':');
        if (!p) continue;
        *p = '\0';
        if (sscanf(p + 1, "%llu %*u %*u %*u %*u %*u %*u %*u %llu", &rx, &tx) >= 2) {
            *rx_total += rx;
            *tx_total += tx;
        }
    }
    fclose(f);
    return 0;
}
 
static void get_ip(char *buf, size_t bufsz) {
    snprintf(buf, bufsz, "0.0.0.0");
    FILE *p = popen("ip -4 route get 1.1.1.1 2>/dev/null", "r");
    if (!p) return;
    char out[512];
    if (fgets(out, sizeof(out), p)) {
        char *src = strstr(out, " src ");
        if (src) {
            src += 5;
            char *end = src;
            while (*end && *end != ' ' && *end != '\n') end++;
            size_t len = (size_t)(end - src);
            if (len > 0 && len < bufsz) {
                memcpy(buf, src, len);
                buf[len] = '\0';
            }
        }
    }
    pclose(p);
}
 
/* ---------- 串口发送/回显 ---------- */
static int open_serial(const char *dev) {
    int fd = open(dev, O_RDWR | O_NOCTTY);
    if (fd < 0) return -1;
    struct termios tty;
    tcgetattr(fd, &tty);
    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_lflag = 0;
    tty.c_oflag = 0;
    tty.c_iflag = 0;
    tcsetattr(fd, TCSANOW, &tty);
    return fd;
}
 
int main(int argc, char **argv) {
    /* 默认串口 /dev/ttyACM0（ESP32 USB CDC），用 "-" 或 "stdout" 表示仅 stdout */
    const char *serial_dev = NULL;
    if (argc >= 2 && argv[1][0] != '\0' && strcmp(argv[1], "-") != 0 && strcmp(argv[1], "stdout") != 0)
        serial_dev = argv[1];
    else if (argc < 2)
        serial_dev = "/dev/ttyACM0";  /* 无参数时默认打开串口，与 01_serial_port_demo 配合 */
    int interval = (argc >= 3) ? atoi(argv[2]) : 1;
    if (interval <= 0) interval = 1;
    int print_echo = 1;
    if (argc >= 4 && (argv[3][0] == '0' || strcmp(argv[3], "q") == 0 || strcmp(argv[3], "quiet") == 0))
        print_echo = 0;
 
    int serial_fd = -1;
    if (serial_dev) {
        serial_fd = open_serial(serial_dev);
        if (serial_fd < 0) {
            perror("serial open failed");
            fprintf(stderr, "fallback: output to stdout only\n");
        } else {
            if (print_echo)
                printf("串口 %s 已打开 115200 8N1，每 %d 秒发送 JSON，回显摘要。按 Ctrl+C 退出。\n", serial_dev, interval);
            else
                printf("串口 %s 已打开 115200 8N1，每 %d 秒发送 JSON（回显已关闭）。按 Ctrl+C 退出。\n", serial_dev, interval);
            fflush(stdout);
        }
    }
 
    unsigned long long prev_idle = 0, prev_total = 0;
    if (read_cpu_idle_total(&prev_idle, &prev_total) != 0) {
        fprintf(stderr, "read /proc/stat failed\n");
        if (serial_fd >= 0) close(serial_fd);
        return 1;
    }
    unsigned long long prev_net_rx = 0, prev_net_tx = 0;
    int net_ok = read_net_bytes(&prev_net_rx, &prev_net_tx);
 
    time_t last_send = time(NULL);
    char line[640];
    char rbuf[256];
 
#define LINEBUF_SIZE 1024
    char linebuf[LINEBUF_SIZE];
    int linepos = 0;
 
    while (1) {
        struct timeval tv = { .tv_sec = 0, .tv_usec = 100000 };
        fd_set rfds;
        FD_ZERO(&rfds);
        if (serial_fd >= 0) FD_SET(serial_fd, &rfds);
        int nfds = (serial_fd >= 0) ? (serial_fd + 1) : 0;
 
        int ret = (nfds > 0) ? select(nfds, &rfds, NULL, NULL, &tv) : 0;
        if (ret > 0 && serial_fd >= 0 && FD_ISSET(serial_fd, &rfds)) {
            ssize_t n = read(serial_fd, rbuf, sizeof(rbuf) - 1);
            if (n > 0) {
                rbuf[n] = '\0';
                int add = (int)n;
                if (linepos + add >= LINEBUF_SIZE - 1) add = LINEBUF_SIZE - 1 - linepos;
                if (add > 0) {
                    memcpy(linebuf + linepos, rbuf, (size_t)add);
                    linepos += add;
                    linebuf[linepos] = '\0';
                }
                /* 按行输出；开启回显时只打摘要（RX/ECHO 字节数），不刷整段 JSON */
                char *p = linebuf;
                while (p < linebuf + linepos) {
                    char *q = strchr(p, '\n');
                    if (!q) break;
                    q++;
                    if ((int)(q - p) <= 1) { /* 空行跳过 */ }
                    else if (!print_echo) { /* 回显关闭：不输出 */ }
                    else if (q - p >= 4 && strncmp(p, "RX: ", 4) == 0)
                        printf("[收到] RX %d 字节\n", (int)(q - p - 4));
                    else if (q - p >= 6 && strncmp(p, "ECHO: ", 6) == 0)
                        printf("[收到] ECHO %d 字节\n", (int)(q - p - 6));
                    else {
                        printf("[收到] %.*s", (int)(q - p), p);
                        fflush(stdout);
                    }
                    if (print_echo) fflush(stdout);
                    {
                        int keep = linepos - (int)(q - linebuf);
                        if (keep > 0)
                            memmove(linebuf, q, (size_t)keep);
                        linepos = keep;
                    }
                    p = linebuf;
                }
            }
        }
 
        time_t now = time(NULL);
        if (now - last_send < interval) continue;
        last_send = now;
 
        /* 采集 */
        unsigned long long idle = 0, total = 0;
        read_cpu_idle_total(&idle, &total);
        unsigned long long di = idle - prev_idle, dt = total - prev_total;
        prev_idle = idle;
        prev_total = total;
        double cpu_usage = (dt > 0) ? (1.0 - (double)di / (double)dt) * 100.0 : 0.0;
 
        double l1 = 0, l5 = 0, l15 = 0;
        read_load(&l1, &l5, &l15);
        int mem_used = 0, mem_total = 0;
        read_mem_mb(&mem_used, &mem_total);
        double temp = read_cpu_temp_c();
        long uptime_s = read_uptime_s();
        long freq_mhz = read_cpu_freq_mhz();
        char ip[64];
        get_ip(ip, sizeof(ip));
 
        double net_rx_kbs = 0.0, net_tx_kbs = 0.0;
        if (net_ok == 0) {
            unsigned long long cur_rx = 0, cur_tx = 0;
            if (read_net_bytes(&cur_rx, &cur_tx) == 0 && interval > 0) {
                net_rx_kbs = (double)(cur_rx - prev_net_rx) / (1024.0 * (double)interval);
                net_tx_kbs = (double)(cur_tx - prev_net_tx) / (1024.0 * (double)interval);
                if (net_rx_kbs < 0) net_rx_kbs = 0;
                if (net_tx_kbs < 0) net_tx_kbs = 0;
                prev_net_rx = cur_rx;
                prev_net_tx = cur_tx;
            }
        }
 
        char tbuf[64];
        struct tm tm;
        time_t t = time(NULL);
        localtime_r(&t, &tm);
        strftime(tbuf, sizeof(tbuf), "%Y-%m-%dT%H:%M:%S", &tm);
 
        char temp_str[32], freq_str[32];
        if (temp < 0) snprintf(temp_str, sizeof(temp_str), "null");
        else snprintf(temp_str, sizeof(temp_str), "%.1f", temp);
        if (freq_mhz < 0) snprintf(freq_str, sizeof(freq_str), "null");
        else snprintf(freq_str, sizeof(freq_str), "%ld", freq_mhz);
 
        int n = snprintf(line, sizeof(line),
            "{\"ts\":\"%s\",\"cpu_usage\":%.1f,\"cpu_temp_c\":%s,"
            "\"mem_used_mb\":%d,\"mem_total_mb\":%d,"
            "\"load1\":%.2f,\"load5\":%.2f,\"load15\":%.2f,"
            "\"ip\":\"%s\",\"uptime_s\":%ld,\"cpu_freq_mhz\":%s,"
            "\"net_rx_kbs\":%.2f,\"net_tx_kbs\":%.2f}\n",
            tbuf, cpu_usage, temp_str,
            mem_used, mem_total,
            l1, l5, l15,
            ip, uptime_s, freq_str,
            net_rx_kbs, net_tx_kbs);
 
        if (n > 0) {
            (void)write(STDOUT_FILENO, line, (size_t)n);
            if (serial_fd >= 0)
                (void)write(serial_fd, line, (size_t)n);
        }
    }
 
    if (serial_fd >= 0) close(serial_fd);
    return 0;
}
