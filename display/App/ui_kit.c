/* ui_kit.c：设置行零件：标题 + 值 + 按钮 */

#include "ui_kit.h"
#include "app_gfx.h"

int kit_row_y(int n)
{
	return KIT_ROW_Y0 + n * KIT_ROW_H;
}

void kit_title(int y, const char *title)
{
	ui_text(KIT_LABEL_X, y + 8, title, C_DIM, UI_BG_NONE);
}

int kit_button(int id, int n, const char *title, const char *value, int on)
{
	int y = kit_row_y(n);

	kit_title(y, title);
	return ui_action_button(id, KIT_VALUE_X, y, KIT_VALUE_W, KIT_VALUE_H,
	                        value, on, 0);
}

int kit_step(int id, int n, const char *title, const char *value,
             const char *minus_text, const char *plus_text)
{
	int y = kit_row_y(n);
	int x_left;
	int x_right;

	x_left  = KIT_VALUE_X;
	x_right = KIT_VALUE_X + KIT_VALUE_W - KIT_STEP_W;

	kit_title(y, title);

	ui_field(0, x_left + KIT_STEP_W + 6, y,
	         KIT_VALUE_W - 2 * KIT_STEP_W - 12, KIT_VALUE_H,
	         value, C_BLUE, C_BG);

	if (ui_action_button(id, x_left, y, KIT_STEP_W, KIT_VALUE_H,
	                     minus_text, 0, 0)) {
		return -1;
	}

	if (ui_action_button(id + 1, x_right, y, KIT_STEP_W, KIT_VALUE_H,
	                     plus_text, 0, 0)) {
		return 1;
	}

	return 0;
}
