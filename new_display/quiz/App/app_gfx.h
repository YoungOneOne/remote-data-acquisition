/* 界面数据、测量结果和绘图接口。业务动作放在 app_ui.c。 */


#ifndef __APP_GFX_H
#define __APP_GFX_H

#include <stdint.h>
#include "app_layout.h"

/* 一帧由采集端发来；通道显示开关单独保存在 g_ui。 */
typedef struct
{
  uint16_t n;
  uint32_t dt_ns; /* 采样间隔，纳秒 */
  uint8_t  scan_code;
  uint16_t ch1[NPT];
  uint16_t ch2[NPT];
  uint8_t  dig[NPT];
} wave_window_t;

#define CHM_CH1     0x01u
#define CHM_CH2     0x02u
#define CHM_DIG0      0x04u
#define CHM_DIG1      0x08u
#define CHM_DIG2      0x10u
#define CHM_ANA     (CHM_CH1 | CHM_CH2)
#define CHM_DIG     (CHM_DIG0 | CHM_DIG1 | CHM_DIG2)

#define TRIGSRC_CH1   SCOPE_TRIG_CH1
#define TRIGSRC_CH2   SCOPE_TRIG_CH2
#define TRIGSRC_DIG0    SCOPE_TRIG_DIG0
#define TRIGSRC_DIG1    SCOPE_TRIG_DIG1
#define TRIGSRC_DIG2    SCOPE_TRIG_DIG2

#define TAB_TRIG      0u
#define TAB_SQW       1u
#define TAB_MEAS      2u

/* 当前界面和采集端配置。 */
typedef struct
{
  uint8_t  scan;
  uint8_t  ch_mask;
  uint8_t  run;
  uint8_t  tab;

  uint8_t  trig_src;
  uint8_t  trig_edge;
  uint16_t trig_level; /* 12位界面刻度，发送前右移4位 */
  uint8_t  trig_mode;

  uint8_t  sqw_on;
  uint32_t sqw_freq;

  uint8_t  link_ok;
  uint32_t frame_count;
  uint16_t lost_frames;
  uint16_t bad_frames;
  uint16_t fps_x10; /* 接收帧率 x10 */
} ui_state_t;

extern ui_state_t g_ui;

typedef struct
{
  float    freq;
  float    vpp;
  float    vavg;
  float    width; /* 秒 */
  float    duty; /* 0..1 */
  uint8_t  valid;
} ui_meas_t;

typedef struct
{
  ui_meas_t ch1;
  ui_meas_t ch2;
  ui_meas_t dig0;
  ui_meas_t dig1;
  ui_meas_t dig2;
} ui_meas_all_t;

void  ui_init(void);
void  ui_draw(const wave_window_t *w);
void  ui_measure(const wave_window_t *w, ui_meas_all_t *m);

#define UI_PAGE_WAVE        0u
#define UI_PAGE_SETTINGS    1u

void    ui_page_set(uint8_t page);
uint8_t ui_page_get(void);

float      ui_code_to_volt(uint16_t code);
const char *ui_scan_label(uint8_t scan);
uint32_t   ui_scan_div_ns(uint8_t scan);


char *ui_str(char *p, const char *s);

char *ui_itoa(char *p, int32_t v);

char *ui_fix(char *p, int32_t v, int dec);

#define UI_BG_NONE      (-1)

void ui_fill(int x, int y, int w, int h, uint16_t color);
void ui_hline(int x0, int x1, int y, uint16_t color);



int  ui_text  (int x, int y, const char *s, uint16_t fg, int32_t bg);

int  ui_text_w  (const char *s);

void ui_text_c  (int cx, int y, const char *s, uint16_t fg, int32_t bg);

void ui_force_redraw(void);

int  ui_text_at(int x, int y, int w, int h, const char *s,
                uint16_t fg, uint16_t bg);

#define UI_FIELD_MAX    8
uint8_t ui_field(uint8_t id, int x, int y, int w, int h,
                 const char *s, uint16_t fg, uint16_t bg);

void ui_field_reset(void);

#define UI_WID_RUN      0
#define UI_WID_SCAN     1
#define UI_WID_CH1      2
#define UI_WID_CH2      3
#define UI_WID_DIG0       4
#define UI_WID_DIG1       5
#define UI_WID_DIG2       6
#define UI_WID_MENU     7
#define UI_WID_MAX      8
#define UI_WID_NONE     (-1)

int  ui_toolbar_hit(int x, int y);

void ui_touch_set(int x, int y, int pressed);

typedef struct
{
	int16_t     x, y, w, h;
	const char *label;
	uint8_t     drawn;
	uint8_t     last_vis;
	uint32_t    gen;
} ui_btn_t;

#define UI_BTN_DEFAULT  {0}

void ui_btn_init(ui_btn_t *b);

int  ui_button(ui_btn_t *b, int x, int y, int w, int h, const char *label, int on);


void ui_btn_invalidate(ui_btn_t *b);

#endif
