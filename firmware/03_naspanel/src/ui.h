// UI 界面模块：LVGL 界面创建和更新
#ifndef UI_H
#define UI_H

#include <lvgl.h>

// NAS 状态数据结构
struct NasStats {
  char ts[20];           // 时间戳
  char ip[16];           // IP 地址
  float cpu_usage;       // CPU 使用率 (%)
  float cpu_temp_c;      // CPU 温度 (°C)
  int mem_used_mb;       // 已用内存 (MB)
  int mem_total_mb;      // 总内存 (MB)
  float load1;           // 1分钟负载
  float load5;           // 5分钟负载
  float load15;          // 15分钟负载
  long uptime_s;         // 运行时间 (秒)
  long cpu_freq_mhz;     // CPU 频率 (MHz)
  float net_rx_kbs;      // 网络接收速率 (KB/s)
  float net_tx_kbs;      // 网络发送速率 (KB/s)
};

void ui_init();                          // 初始化界面元素
void ui_update(const NasStats &stats);   // 更新界面显示

#endif
