/**
  ******************************************************************************
  * @file    app_ui.h
  * @brief   显示端界面
  ******************************************************************************
  */

#ifndef __APP_UI_H
#define __APP_UI_H

#include <stdint.h>
#include "app_gfx.h"            /* wave_window_t */

/* 在 ui_init() 之后设置界面默认值。 */
void app_ui_init(void);
void app_ui_draw_settings(void);

/* 工具条按钮按下后的动作。wid 传 UI_WID_* (ui_toolbar_hit() 的返回值) */
void app_ui_widget(int wid);
uint8_t app_ui_needs_refresh(void);


#endif /* __APP_UI_H */
