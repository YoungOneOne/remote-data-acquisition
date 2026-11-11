#ifndef __APP_UI_H
#define __APP_UI_H

#ifndef APP_DEBUG_TOUCH
#define APP_DEBUG_TOUCH     0
#endif

#include <stdint.h>
#include "app_gfx.h"

void app_ui_init(void);

void app_ui_widget(int wid);

#endif
