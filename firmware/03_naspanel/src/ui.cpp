// UI 界面实现：创建和更新 LVGL 标签
#include "ui.h"
#include <stdio.h>

// LVGL 对象
static lv_obj_t *panel;        // 容器图层
static lv_obj_t *label_ip;     // IP 地址标签
static lv_obj_t *label_cpu;    // CPU 使用率标签
static lv_obj_t *label_temp;   // 温度标签
static lv_obj_t *label_net;    // 网络速率标签

// 初始化界面：创建所有标签并设置初始文本
void ui_init() {
  // 创建容器图层
  panel = lv_obj_create(lv_scr_act());
  lv_obj_set_size(panel, 230, 270);  // 设置尺寸：宽 220px，高 260px
  lv_obj_set_pos(panel, 5, 5);     // 设置位置：距左边 10px，距顶部 10px
  lv_obj_set_style_radius(panel, 30, 0);  // 设置圆角半径：10px
  lv_obj_set_style_bg_color(panel, lv_color_make(111, 143, 143), 0); // 深天蓝背景/
  lv_label_set_text(panel, "IP: --");
  


  // 在图层上创建标签（父对象改为 panel）
  label_ip = lv_label_create(panel);
  lv_obj_set_pos(label_ip, 10, 10);
  lv_label_set_text(label_ip, "IP: --");

  label_cpu = lv_label_create(panel);
  lv_obj_set_pos(label_cpu, 10, 40);
  lv_label_set_text(label_cpu, "CPU: --%");

  label_temp = lv_label_create(panel);
  lv_obj_set_pos(label_temp, 10, 70);
  lv_label_set_text(label_temp, "Ti: --C");

  label_net = lv_label_create(panel);
  lv_obj_set_pos(label_net, 10, 100);
  lv_label_set_text(label_net, "NET: --/--");
}

// 更新界面：根据 NAS 状态数据刷新标签内容
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
