#ifndef __UI_KIT_H
#define __UI_KIT_H

#include <stdint.h>
#include "app_gfx.h"

#define KIT_ROW_Y0      64
#define KIT_ROW_H       38
#define KIT_LABEL_X     18
#define KIT_VALUE_X     178
#define KIT_VALUE_W     280
#define KIT_VALUE_H     30
#define KIT_STEP_W      52
#define KIT_STEP_GAP    178

int kit_row_y(int n);

void kit_title(int y, const char *title);

int kit_button(int id, int n, const char *title, const char *value, int on);

int kit_step(int id, int n, const char *title, const char *value,
             const char *minus_text, const char *plus_text);

#endif
