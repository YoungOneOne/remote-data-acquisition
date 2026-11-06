/* 主界面：顶栏、测量摘要、底部八个按键。 */
#ifndef APP_SCREEN_H
#define APP_SCREEN_H
#include "app_gfx.h"
#define SCR_KEYS_N 8
extern const uint8_t g_keys[SCR_KEYS_N];
void scr_status_bar(const wave_window_t *w);
void scr_meas_line(const wave_window_t *w);
void scr_softkeys(void);
int scr_softkey_hit(int x, int y);
#endif
