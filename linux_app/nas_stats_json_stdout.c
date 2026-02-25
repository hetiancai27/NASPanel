/*
 * nas_stats_json_stdout.c - 采集 NAS 系统状态并输出到 stdout（每行一条 JSON）
 *
 * 该程序只输出到 stdout，不涉及串口；可用于管道/日志采集，或供其他程序转发。
 *
 * 构建（示例）：
 *   gcc -O2 -o nas_usb_stats nas_stats_json_stdout.c
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
 
static int read_cpu_idle_total(unsigned long long *idle, unsigned long long *total) {
    FILE *f = fopen("/proc/stat", "r");
    if (!f) return -1;
 
    // cpu  user nice system idle iowait irq softirq steal
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
    char key[64];
    long val;
    char unit[32];
    while (fscanf(f, "%63s %ld %31s", key, &val, unit) == 3) {
        if (strcmp(key, "MemTotal:") == 0) mt = val;
        else if (strcmp(key, "MemAvailable:") == 0) ma = val;
        if (mt && ma) break;
    }
    fclose(f);
    if (!mt || !ma) return -1;
 
    long used_kb = mt - ma;
    *used_mb = (int)(used_kb / 1024);
    *total_mb = (int)(mt / 1024);
    return 0;
}
 
static double read_cpu_temp_c(void) {
    // 返回 <0 表示读不到
    // 只尝试第一个 thermal_zone
    char path[128];
    for (int i=0; i<16; i++) {
        snprintf(path, sizeof(path), "/sys/class/thermal/thermal_zone%d/temp", i);
        FILE *f = fopen(path, "r");
        if (!f) continue;
        long v=0;
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
    double up=0;
    fscanf(f, "%lf", &up);
    fclose(f);
    return (long)up;
}
 
static long read_cpu_freq_mhz(void) {
    // cpufreq 优先
    FILE *f = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", "r");
    if (f) {
        long khz=0;
        if (fscanf(f, "%ld", &khz)==1 && khz>0) { fclose(f); return khz/1000; }
        fclose(f);
    }
 
    // fallback: /proc/cpuinfo 平均 cpu MHz
    f = fopen("/proc/cpuinfo", "r");
    if (!f) return -1;
    char *line = NULL; size_t n=0;
    double sum=0; int cnt=0;
    while (getline(&line, &n, f) > 0) {
        if (strncasecmp(line, "cpu MHz", 7)==0) {
            char *p = strchr(line, ':');
            if (p) { sum += atof(p+1); cnt++; }
        }
    }
    free(line);
    fclose(f);
    if (cnt>0) return (long)(sum/cnt);
    return -1;
}
 
/* 从 /proc/net/dev 汇总所有接口的收发字节数 */
static int read_net_bytes(unsigned long long *rx_total, unsigned long long *tx_total) {
    FILE *f = fopen("/proc/net/dev", "r");
    if (!f) return -1;
    char line[512];
    *rx_total = 0;
    *tx_total = 0;
    /* 跳过前两行表头 */
    if (!fgets(line, sizeof(line), f) || !fgets(line, sizeof(line), f)) {
        fclose(f);
        return -1;
    }
    while (fgets(line, sizeof(line), f)) {
        unsigned long long rx = 0, tx = 0;
        char *p = strchr(line, ':');
        if (!p) continue;
        *p = '\0';
        if (sscanf(p + 1, "%llu %*u %*u %*u %*u %*u %*u %*u %llu",
                   &rx, &tx) >= 2) {
            *rx_total += rx;
            *tx_total += tx;
        }
    }
    fclose(f);
    return 0;
}
 
static void get_ip(char *buf, size_t bufsz) {
    // 简化实现：调用 ip route get
    // 输出格式里有 "src x.x.x.x"
    snprintf(buf, bufsz, "0.0.0.0");
    FILE *p = popen("ip -4 route get 1.1.1.1 2>/dev/null", "r");
    if (!p) return;
    char out[512];
    if (fgets(out, sizeof(out), p)) {
        char *src = strstr(out, " src ");
        if (src) {
            src += 5;
            char *end = src;
            while (*end && *end!=' ' && *end!='\n') end++;
            size_t len = (size_t)(end - src);
            if (len > 0 && len < bufsz) {
                memcpy(buf, src, len);
                buf[len] = '\0';
            }
        }
    }
    pclose(p);
}
 
int main(int argc, char **argv) {
    // 只生成并输出 JSON（stdout）
    // 为了兼容旧调用方式：仍接受 argv[1]=dev, argv[2]=interval，但 dev 会被忽略
    const char *dev = (argc >= 2) ? argv[1] : "(ignored)";
    int interval = (argc >= 3) ? atoi(argv[2]) : 1;
    if (interval <= 0) interval = 1;
 
    (void)dev;
    int fd = STDOUT_FILENO;
 
    unsigned long long prev_idle=0, prev_total=0;
    if (read_cpu_idle_total(&prev_idle, &prev_total) != 0) {
        fprintf(stderr, "read /proc/stat failed\n");
        return 1;
    }
 
    unsigned long long prev_net_rx = 0, prev_net_tx = 0;
    int net_ok = read_net_bytes(&prev_net_rx, &prev_net_tx);
 
    while (1) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
 
        unsigned long long idle=0,total=0;
        read_cpu_idle_total(&idle, &total);
        unsigned long long di = idle - prev_idle;
        unsigned long long dt = total - prev_total;
        prev_idle = idle; prev_total = total;
 
        double cpu_usage = 0.0;
        if (dt > 0) cpu_usage = (1.0 - (double)di/(double)dt) * 100.0;
 
        double l1=0,l5=0,l15=0;
        read_load(&l1,&l5,&l15);
 
        int mem_used=0, mem_total=0;
        read_mem_mb(&mem_used,&mem_total);
 
        double temp = read_cpu_temp_c();
        long uptime_s = read_uptime_s();
        long freq_mhz = read_cpu_freq_mhz();
 
        char ip[64]; get_ip(ip, sizeof(ip));
 
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
 
        // ISO time (seconds)
        char tbuf[64];
        time_t now = time(NULL);
        struct tm tm; localtime_r(&now, &tm);
        strftime(tbuf, sizeof(tbuf), "%Y-%m-%dT%H:%M:%S", &tm);
 
        // temp / freq 可能无：用 null
        char temp_str[32], freq_str[32];
        if (temp < 0) snprintf(temp_str, sizeof(temp_str), "null");
        else snprintf(temp_str, sizeof(temp_str), "%.1f", temp);
        if (freq_mhz < 0) snprintf(freq_str, sizeof(freq_str), "null");
        else snprintf(freq_str, sizeof(freq_str), "%ld", freq_mhz);
 
        char line[640];
        int n = snprintf(line, sizeof(line),
            "{\"ts\":\"%s\",\"cpu_usage\":%.1f,\"cpu_temp_c\":%s,"
            "\"mem_used_mb\":%d,\"mem_total_mb\":%d,"
            "\"load1\":%.2f,\"load5\":%.2f,\"load15\":%.2f,"
            "\"ip\":\"%s\",\"uptime_s\":%ld,\"cpu_freq_mhz\":%s,"
            "\"net_rx_kbs\":%.2f,\"net_tx_kbs\":%.2f}\n",
            tbuf, cpu_usage, temp_str,
            mem_used, mem_total,
            l1,l5,l15,
            ip, uptime_s, freq_str,
            net_rx_kbs, net_tx_kbs
        );
 
        if (n > 0) {
            (void)write(fd, line, (size_t)n);
        }
 
        sleep(interval);
    }
 
    return 0;
}
