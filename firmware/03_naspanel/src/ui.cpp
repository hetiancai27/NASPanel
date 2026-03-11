#include "ui.h"
#include <stdio.h>

static lv_obj_t *label_ip;
static lv_obj_t *label_cpu;
static lv_obj_t *label_temp;
static lv_obj_t *label_net;

void ui_init() {
  label_ip = lv_label_create(lv_scr_act());
  lv_obj_set_pos(label_ip, 20, 20);
  lv_label_set_text(label_ip, "IP: --");

  label_cpu = lv_label_create(lv_scr_act());
  lv_obj_set_pos(label_cpu, 20, 50);
  lv_label_set_text(label_cpu, "CPU: --%");

  label_temp = lv_label_create(lv_scr_act());
  lv_obj_set_pos(label_temp, 20, 80);
  lv_label_set_text(label_temp, "Ti: --C");

  label_net = lv_label_create(lv_scr_act());
  lv_obj_set_pos(label_net, 20, 110);
  lv_label_set_text(label_net, "NET: --/--");
}

void ui_update(const NasStats &s) {
  static char buf[64];

  snprintf(buf, sizeof(buf), "IP: %s", s.ip);
  lv_label_set_text(label_ip, buf);

  snprintf(buf, sizeof(buf), "CPU: %.1f%%", s.cpu_usage);
  lv_label_set_text(label_cpu, buf);

  snprintf(buf, sizeof(buf), "T: %.1fC", s.cpu_temp_c);
  lv_label_set_text(label_temp, buf);

  snprintf(buf, sizeof(buf), "NET: %.1f/%.1f", s.net_rx_kbs, s.net_tx_kbs);
  lv_label_set_text(label_net, buf);
}
