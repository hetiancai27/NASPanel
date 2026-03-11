#ifndef UI_H
#define UI_H

#include <lvgl.h>

struct NasStats {
  char ts[20];
  char ip[16];
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

void ui_init();
void ui_update(const NasStats &stats);

#endif
