#include <lvgl.h>             // 引入 LVGL 图形库头文件
#include <Adafruit_GFX.h>     // 引入 Adafruit 图形库基类
#include <Adafruit_ST7789.h>  // 引入 Adafruit 的 ST7789 显示驱动库
#include <SPI.h>              // 引入 SPI 通信库

// ==== 定义连接屏幕的引脚 ====
#define TFT_CS 9    // TFT 屏幕的片选引脚
#define TFT_DC 7    // 数据/命令选择引脚
#define TFT_RST 8   // 复位引脚
#define TFT_MOSI 6  // SPI MOSI 引脚（主出从入）
#define TFT_SCLK 5  // SPI 时钟引脚
#define TFT_BL 10   // 背光控制引脚

// ==== 定义屏幕的分辨率 ====
#define SCREEN_WIDTH 240    // 屏幕宽度为 76 像素
#define SCREEN_HEIGHT 296  // 屏幕高度为 284 像素

// 创建一个 ST7789 显示对象（使用指定的引脚）
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// ==== 定义 LVGL 显示缓冲 ====
static lv_disp_draw_buf_t draw_buf;        // LVGL 显示缓冲区对象
static lv_color_t buf[SCREEN_WIDTH * 10];  // 实际用于存储颜色数据的缓冲区（高度为10像素）

// lv_color_t lv_color_from_rgb(uint8_t r, uint8_t g, uint8_t b, bool invert = false) {
//   if (invert) {
//     r = 255 - r;
//     g = 255 - g;
//     b = 255 - b;
//   }

//   return lv_color_make(r, g, b);  // 正常构造 lv_color_t
// }


// ==== LVGL 刷新显示的回调函数（将缓冲区内容刷到屏幕）====
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  tft.startWrite();                                   // 开始 SPI 写入
  for (int16_t y = area->y1; y <= area->y2; y++) {    // 遍历区域内的每一行
    for (int16_t x = area->x1; x <= area->x2; x++) {  // 遍历该行内的每一个像素
                                                      //uint16_t color = color_p->full;                 // 获取当前像素的颜色值
      uint16_t color = lv_color_to16(*color_p);
      tft.writePixel(x, y, color);  // 写入该像素到屏幕
      color_p++;                    // 指向下一个颜色数据
    }
  }
  tft.endWrite();             // 结束 SPI 写入
  lv_disp_flush_ready(disp);  // 通知 LVGL 已经刷新完成
}

// ==== 初始化 LVGL 和显示驱动 ====
void lvgl_setup() {
  lv_init();  // 初始化 LVGL 库

  lv_disp_draw_buf_init(&draw_buf, buf, NULL, SCREEN_WIDTH * 10);  // 初始化显示缓冲

  static lv_disp_drv_t disp_drv;      // 创建一个显示驱动对象
  lv_disp_drv_init(&disp_drv);        // 初始化显示驱动对象
  disp_drv.hor_res = SCREEN_WIDTH;    // 设置水平分辨率
  disp_drv.ver_res = SCREEN_HEIGHT;   // 设置垂直分辨率
  disp_drv.flush_cb = my_disp_flush;  // 设置刷新回调函数
  disp_drv.draw_buf = &draw_buf;      // 设置使用的缓冲区

  lv_disp_drv_register(&disp_drv);  // 注册显示驱动到 LVGL
}

void lvgl_ui() {
  // 获取屏幕根对象
  lv_obj_t *bg = lv_scr_act();
  lv_obj_set_size(bg, SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(bg, lv_color_make(0, 0, 0), 0);

  // 背景图层
  lv_obj_t *gr = lv_obj_create(lv_scr_act());
  lv_obj_set_size(gr, SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_obj_clear_flag(gr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_border_width(gr, 0, 0);
  lv_obj_set_style_bg_color(gr, lv_color_make(0, 191, 255), 0);

  // 顶部内容区域
  lv_obj_t *content = lv_obj_create(lv_scr_act());
  lv_obj_set_size(content, SCREEN_WIDTH, 190);
  lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(content, lv_color_make(0, 191, 255), 0);
  lv_obj_set_style_border_width(content, 0, 0);
  lv_obj_set_style_bg_opa(content, LV_OPA_50, LV_PART_MAIN);

  // 创建圆弧
  lv_obj_t *arc = lv_arc_create(content);
  lv_obj_set_size(arc, 70, 70);
  lv_obj_align(arc, LV_ALIGN_CENTER, 0, -48);

  lv_arc_set_range(arc, 0, 100);
  lv_arc_set_value(arc, 75);
  lv_arc_set_bg_angles(arc, 135, 45);
  lv_arc_set_rotation(arc, 0);
  lv_arc_set_mode(arc, LV_ARC_MODE_NORMAL);      // 禁用 knob 模式
  lv_obj_remove_style(arc, NULL, LV_PART_KNOB);  // 移除蓝点

  // 弧线样式
  lv_obj_set_style_arc_color(arc, lv_palette_main(LV_PALETTE_GREEN), LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(arc, 8, LV_PART_MAIN);
  lv_obj_set_style_arc_width(arc, 8, LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(arc, false, LV_PART_MAIN);
  lv_obj_set_style_arc_rounded(arc, false, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);
  lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);

  // 圆弧中心百分比
  lv_obj_t *label_percent = lv_label_create(arc);
  lv_label_set_text_fmt(label_percent, "%d%%", lv_arc_get_value(arc));
  lv_obj_center(label_percent);

  // 圆弧下方显示 CPU 文本
  lv_obj_t *label_cpu = lv_label_create(content);
  lv_label_set_text(label_cpu, "CPU");
  lv_obj_set_style_text_font(label_cpu, &lv_font_montserrat_12, 0);  // 设置字体大小
  lv_obj_align_to(label_cpu, arc, LV_ALIGN_OUT_BOTTOM_MID, 0, -18);

  // === 添加 RAM 圆弧 ===
  lv_obj_t *arc_ram = lv_arc_create(content);
  lv_obj_set_size(arc_ram, 70, 70);
  lv_obj_align(arc_ram, LV_ALIGN_CENTER, 0, 48);  // 相对于中心向下偏移

  lv_arc_set_range(arc_ram, 0, 100);
  lv_arc_set_value(arc_ram, 50);  // 假设 RAM 使用率为 50%
  lv_arc_set_bg_angles(arc_ram, 135, 45);
  lv_arc_set_rotation(arc_ram, 0);
  lv_arc_set_mode(arc_ram, LV_ARC_MODE_NORMAL);
  lv_obj_remove_style(arc_ram, NULL, LV_PART_KNOB);

  lv_obj_set_style_arc_color(arc_ram, lv_palette_main(LV_PALETTE_BLUE), LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(arc_ram, 8, LV_PART_MAIN);
  lv_obj_set_style_arc_width(arc_ram, 8, LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(arc_ram, true, LV_PART_MAIN);
  lv_obj_set_style_arc_rounded(arc_ram, true, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(arc_ram, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(arc_ram, LV_OBJ_FLAG_CLICKABLE);

  // 圆弧中心显示 RAM 百分比
  lv_obj_t *label_ram_percent = lv_label_create(arc_ram);
  lv_label_set_text_fmt(label_ram_percent, "%d%%", lv_arc_get_value(arc_ram));
  lv_obj_center(label_ram_percent);

  // 圆弧上方显示 "RAM"
  lv_obj_t *label_ram = lv_label_create(content);
  lv_label_set_text(label_ram, "RAM");
  lv_obj_set_style_text_font(label_ram, &lv_font_montserrat_12, 0);
  lv_obj_align_to(label_ram, arc_ram, LV_ALIGN_OUT_TOP_MID, 0, 68);  // 向上偏移显示

// ========== 创建下方网速面板 ==========

lv_obj_t *net_panel = lv_obj_create(lv_scr_act());
lv_obj_set_size(net_panel, 76, 94);
lv_obj_align_to(net_panel, content, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
lv_obj_clear_flag(net_panel, LV_OBJ_FLAG_SCROLLABLE);
lv_obj_set_style_bg_color(net_panel, lv_color_make(255, 165, 0), 0);  // 橙色背景
lv_obj_set_style_bg_opa(net_panel, LV_OPA_COVER, LV_PART_MAIN);
lv_obj_set_style_border_width(net_panel, 0, 0);  // 无边框

// ========== 上传箭头和速度 ==========

// 上传箭头
lv_obj_t *label_up_icon = lv_label_create(net_panel);
lv_label_set_text(label_up_icon, LV_SYMBOL_UPLOAD);
lv_obj_set_style_text_font(label_up_icon, &lv_font_montserrat_10, 0);
lv_obj_set_style_text_color(label_up_icon, lv_palette_main(LV_PALETTE_GREEN), 0);
lv_obj_align(label_up_icon, LV_ALIGN_TOP_LEFT, -6, 6);

// 上传速度
lv_obj_t *label_up_speed = lv_label_create(net_panel);
lv_label_set_text(label_up_speed, "123 kB/s");
lv_obj_set_style_text_font(label_up_speed, &lv_font_montserrat_10, 0);
lv_obj_set_style_text_color(label_up_speed, lv_color_white(), 0);
lv_obj_align_to(label_up_speed, label_up_icon, LV_ALIGN_OUT_RIGHT_MID, 6, 0);

// ========== 下载箭头和速度 ==========

// 下载箭头
lv_obj_t *label_down_icon = lv_label_create(net_panel);
lv_label_set_text(label_down_icon, LV_SYMBOL_DOWNLOAD);
lv_obj_set_style_text_font(label_down_icon, &lv_font_montserrat_10, 0);
lv_obj_set_style_text_color(label_down_icon, lv_palette_main(LV_PALETTE_BLUE), 0);
lv_obj_align_to(label_down_icon, label_up_icon, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 12);

// 下载速度
lv_obj_t *label_down_speed = lv_label_create(net_panel);
lv_label_set_text(label_down_speed, "456 kB/s");
lv_obj_set_style_text_font(label_down_speed, &lv_font_montserrat_10, 0);
lv_obj_set_style_text_color(label_down_speed, lv_color_white(), 0);
lv_obj_align_to(label_down_speed, label_down_icon, LV_ALIGN_OUT_RIGHT_MID, 6, 0);

}



// ==== Arduino 的初始化函数 ====
void setup() {
  pinMode(TFT_BL, OUTPUT);     // 设置背光引脚为输出模式
  digitalWrite(TFT_BL, HIGH);  // 打开背光（高电平）

  SPI.begin(TFT_SCLK, -1, TFT_MOSI, -1);  // 初始化 SPI，使用指定的时钟和 MOSI 引脚
  tft.init(SCREEN_WIDTH, SCREEN_HEIGHT);  // 初始化 TFT 屏幕，设置宽高
  tft.setRotation(2);                     // 设置屏幕旋转方向（根据实际安装方向调整）
  tft.fillScreen(ST77XX_WHITE);           // 将屏幕背景清空为黑色
  //tft.invertDisplay(true);  // 全屏反转颜色
  tft.invertDisplay(false);
  lvgl_setup();  // 初始化 LVGL 及其显示驱动
  lvgl_ui();     // 创建 UI 元素
  lv_timer_handler();
}

// ==== Arduino 主循环函数 ====
unsigned long last_tick = 0;

void loop() {
  lv_timer_handler();  // 调用 LVGL 的定时器处理函数，维持 GUI 正常运作
  delay(5);            // 稍作延时，避免 CPU 占用过高
}
