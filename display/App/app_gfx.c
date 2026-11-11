/* app_gfx.c：显示引擎：帧缓冲、翻页、控件、开机画面 */

#include "app_gfx.h"
#include "app_layout.h"
#include "app_screen.h"
#include "drv_lcd.h"
#include "drv_font.h"
#include "gfx_priv.h"
#include <string.h>
#include <stdio.h>

ui_state_t g_ui;

uint16_t *s_fb;
static uint16_t *s_show;

uint8_t   s_double;

#define FB_SRAM         ((uint16_t *)0x20010000u)

static wave_window_t s_prev;
static uint8_t       s_prev_mask;
static uint8_t       s_prev_valid;

static uint32_t s_chrome_sig;
static uint8_t  s_chrome_valid;

static uint8_t s_page = UI_PAGE_WAVE;

static void (*s_custom)(void);

static int s_tx = -1, s_ty = -1, s_tp;

static uint8_t s_touch_consumed;

static uint32_t s_draw_gen;

static uint16_t *s_fbs[3];
static uint8_t   s_fb_idx;

#define UI_SKIP_VSYNC       1

static void flip(void)
{
	int k;

	if (!s_double)
	{
		for (k = 0; k < UI_SKIP_VSYNC; k++)
			drv_lcd_wait_vsync();
		return;
	}

	drv_lcd_set_fb_nowait(s_fb);

	for (k = 0; k < UI_SKIP_VSYNC; k++)
		drv_lcd_wait_vsync();

	s_fb_idx = (uint8_t)((s_fb_idx + 1) % 3u);
	s_fb     = s_fbs[s_fb_idx];

	s_draw_gen++;
}

void ui_init(void)
{
	memset(&g_ui, 0, sizeof(g_ui));

	if (g_sdram_ok)
	{
		s_fbs[0] = (uint16_t *)FB0_ADDR;
		s_fbs[1] = (uint16_t *)FB1_ADDR;
		s_fbs[2] = (uint16_t *)FB2_ADDR;

		s_fb_idx = 1;
		s_fb     = s_fbs[1];
		s_show   = s_fbs[0];
		s_double = 1u;
	}
	else
	{
		s_fbs[0] = FB_SRAM;
		s_fbs[1] = FB_SRAM;
		s_fbs[2] = FB_SRAM;

		s_double = 0u;
		s_fb = s_show = FB_SRAM;
	}

	(void)drv_lcd_init(s_show);

	bg_tables_init();
	bg_at_update_trig();

	{
		uint16_t *save = s_fb;
		int       k, n;
		if (s_double != 0) {
		    n = 3;
		} else {
		    n = 1;
		}

		for (k = 0; k < n; k++)
		{
			s_fb = s_fbs[k];
			fillrect(0, 0, (int)SCR_W, (int)SCR_H, C_BG);
		}
		s_fb = save;
	}

	draw_grid();
	draw_trig();
	scr_toolbar(0);

	s_chrome_valid = 0;

	s_prev_valid = 0;

	drv_lcd_set_fb(s_show);

	printf("ui: g_sdram_ok=%d s_double=%d fb=%p\r\n",
	       (int)g_sdram_ok, (int)s_double, (void *)s_fb);

}

static uint32_t chrome_sig(const wave_window_t *w)
{
	uint32_t s = 0;

	s = s * 31u + g_ui.run;
	s = s * 31u + g_ui.scan;
	s = s * 31u + w->scan_code;
	s = s * 31u + g_ui.ch_mask;

	s = s * 31u + g_ui.trig_level;
	s = s * 31u + g_ui.trig_src;
	s = s * 31u + g_ui.trig_edge;
	s = s * 31u + g_ui.trig_mode;
	return s;
}

static void erase_channel(const wave_window_t *prev, uint8_t prev_mask, uint8_t bit)
{
	if (prev_mask & bit)
	{
		s_restore = 1;
		switch (bit)
		{
			case CHM_CH1: trace_ana(prev, prev->ch1, C_BG); break;
			case CHM_CH2: trace_ana(prev, prev->ch2, C_BG); break;
			case CHM_DIG0:  trace_dig(prev, 0, 0, C_BG);      break;
			case CHM_DIG1:  trace_dig(prev, 1, 1, C_BG);      break;
			case CHM_DIG2:  trace_dig(prev, 2, 2, C_BG);      break;
			default: break;
		}
		s_restore = 0;
	}

}

void ui_touch_set(int x, int y, int pressed)
{
    if (!pressed || !s_tp) s_touch_consumed = 0;
    s_tx = x;
    s_ty = y;
    s_tp = pressed;
}

int ui_toolbar_hit(int x, int y)
{
	int i;

	if (s_page != UI_PAGE_WAVE)
		return UI_WID_NONE;

	i = scr_softkey_hit(x, y);

	if (i < 0) {
	    return UI_WID_NONE;
	}
	return i;
}

void ui_btn_init(ui_btn_t *b)
{
	static const ui_btn_t def = UI_BTN_DEFAULT;

	if (b != 0)
		*b = def;
}

void ui_btn_invalidate(ui_btn_t *b)
{
	if (b != 0)
		b->drawn = 0;
}

static ui_btn_t s_action_btn[UI_ACTION_BTN_MAX];
static uint8_t  s_action_btn_ready;

static void action_btn_init_all(void)
{
	int i;

	if (s_action_btn_ready != 0)
		return;

	for (i = 0; i < UI_ACTION_BTN_MAX; i++)
		ui_btn_init(&s_action_btn[i]);

	s_action_btn_ready = 1;
}

static void action_btn_invalidate_all(void)
{
	int i;

	action_btn_init_all();

	for (i = 0; i < UI_ACTION_BTN_MAX; i++)
		ui_btn_invalidate(&s_action_btn[i]);
}

int ui_action_button(int id, int x, int y, int w, int h,
                     const char *label, int on, void (*action)(void))
{
	int clicked;

	if (id < 0 || id >= UI_ACTION_BTN_MAX)
		return 0;

	action_btn_init_all();

	clicked = ui_button(&s_action_btn[id], x, y, w, h, label, on);

	if (clicked != 0) {
		if (action != 0)
			action();
		return 1;
	}

	return 0;
}

int ui_action_button_color(int id, int x, int y, int w, int h,
                           const char *label, int on,
                           uint16_t bg, uint16_t fg,
                           uint16_t bg_on, uint16_t fg_on,
                           void (*action)(void))
{
	int clicked;

	if (id < 0 || id >= UI_ACTION_BTN_MAX)
		return 0;

	action_btn_init_all();

	s_action_btn[id].bg    = bg;
	s_action_btn[id].fg    = fg;
	s_action_btn[id].bg_on = bg_on;
	s_action_btn[id].fg_on = fg_on;

	clicked = ui_button(&s_action_btn[id], x, y, w, h, label, on);

	if (clicked != 0) {
		if (action != 0)
			action();
		return 1;
	}

	return 0;
}

static void (*s_sk_fn)(int wid) = 0;

void ui_set_softkey_handler(void (*fn)(int wid))
{
	s_sk_fn = fn;
}

void ui_softkey_dispatch(int wid)
{
	if (s_sk_fn != 0)
		s_sk_fn(wid);
}

static void btn_paint(const ui_btn_t *b, int vis)
{
	uint16_t bg, fg;
	if (vis != 0) {
	    bg = b->bg_on;
	    fg = b->fg_on;
	} else {
	    bg = b->bg;
	    fg = b->fg;
	}

	fillrect(b->x, b->y, b->w, b->h, bg);
	if (vis != 0) {
	    rect_(b->x, b->y, b->w, b->h, b->fg_on);
	} else {
	    rect_(b->x, b->y, b->w, b->h, C_EDGE);
	}

	if (b->label != 0)
		txt_c(&Font12, b->x + b->w / 2,
		      b->y + (b->h - F12H) / 2, b->label, fg, (int32_t)bg);
}

int ui_button(ui_btn_t *b, int x, int y, int w, int h, const char *label, int on)
{
	int inside, vis, click = 0;

	if (b == 0 || w <= 0 || h <= 0)
		return 0;

    if (b->label != label || b->x != x || b->y != y || b->w != w || b->h != h)
        b->drawn = 0;

	b->x     = (int16_t)x;
	b->y     = (int16_t)y;
	b->w     = (int16_t)w;
	b->h     = (int16_t)h;
	b->label = label;
	if (on != 0) {
	    b->on = 1;
	} else {
	    b->on = 0;
	}

	inside = (s_tp && s_tx >= x && s_tx < x + w && s_ty >= y && s_ty < y + h);

	if (inside)
	{
		b->down = 1;

		if (!s_touch_consumed)
		{
			s_touch_consumed = 1;
			click = 1;
		}
	}
	else
	{
		b->down = 0;
	}

	if (b->on != 0 || b->down != 0) {
	    vis = 1;
	} else {
	    vis = 0;
	}
	if (!b->drawn || vis != (int)b->last_vis || b->gen != s_draw_gen)
	{
		btn_paint(b, vis);
		b->drawn    = 1;
		b->last_vis = (uint8_t)vis;
		b->gen      = s_draw_gen;
	}

	return click;
}

void ui_page_set(uint8_t page)
{
	if (page != UI_PAGE_SETTINGS) {
	    page = UI_PAGE_WAVE;
	}

	if (page == s_page)
		return;

	s_page = page;
    s_touch_consumed = 1;

	ui_force_redraw();
	action_btn_invalidate_all();

	if (page == UI_PAGE_WAVE)
		s_prev_valid = 0;
}

uint8_t ui_page_get(void)
{
	return s_page;
}

void ui_draw(const wave_window_t *w)
{

	if (s_fb == 0) ui_init();

	if (s_page == UI_PAGE_SETTINGS)
	{
        if (s_double || !s_chrome_valid) {
            fillrect(0, 0, SCR_W, SCR_H, C_BG);
            ui_field_reset();
            s_chrome_valid = 1;
        }
		if (s_custom != 0) s_custom();

		flip();
		return;
	}

	if (w == 0 || w->n < 2) return;

	if (s_double)
	{

		fillrect(0, 0, (int)SCR_W, (int)SCR_H, C_BG);
		draw_grid();
		draw_trig();
		scr_status_bar(w);
		scr_toolbar(w);

		draw_traces(w);

		ui_field_reset();

		if (s_custom != 0) s_custom();

		flip();
		return;
	}

	drv_lcd_wait_vsync();

    if (s_prev_valid) {
        erase_channel(&s_prev, s_prev_mask, CHM_CH1);
        erase_channel(&s_prev, s_prev_mask, CHM_CH2);
        erase_channel(&s_prev, s_prev_mask, CHM_DIG0);
        erase_channel(&s_prev, s_prev_mask, CHM_DIG1);
        erase_channel(&s_prev, s_prev_mask, CHM_DIG2);
    }
    draw_traces(w);

	uint32_t sig = chrome_sig(w);

	if (!s_chrome_valid || sig != s_chrome_sig)
	{
		s_chrome_sig   = sig;
		s_chrome_valid = 1;

		bg_at_update_trig();
		scr_status_bar(w);
		scr_toolbar(w);

		draw_grid();
		draw_trig();

		draw_traces(w);
	}

	scr_status_bar(w);

	scr_meas_line(w);

	if (s_custom != 0)
		s_custom();

	memcpy(&s_prev, w, sizeof(s_prev));
	s_prev_mask  = g_ui.ch_mask;
	s_prev_valid = 1;

	flip();
}

char *ui_str(char *p, const char *s)
{
	return put_s(p, s);
}

char *ui_itoa(char *p, int32_t v)
{
	uint32_t u;
	char     t[12];
	int      n = 0;

	if (p == 0) return 0;

	if (v < 0)
	{
		*p++ = '-';
		u = (uint32_t)(-v);
	}
	else
	{
		u = (uint32_t)v;
	}

	do { t[n++] = (char)('0' + (u % 10u)); u /= 10u; } while (u != 0u && n < 12);
	while (n) *p++ = t[--n];
	*p = 0;
	return p;
}

char *ui_fix(char *p, int32_t v, int dec)
{
	return put_fix(p, v, dec);
}

void ui_set_custom(void (*fn)(void))
{
	s_custom = fn;
}

int ui_text_at(int x, int y, int w, int h, const char *s, uint16_t fg, uint16_t bg)
{
	int wpx = tw(&Font12, s);

	if (wpx > 0) wpx -= 1;

	fillrect(x, y, w, h, bg);
	txt(&Font12, x + (w - wpx) / 2, y + (h - F12H) / 2, s, fg, (int32_t)bg);
	return wpx;
}

static char s_field_last[UI_FIELD_MAX][24];

void ui_field_reset(void)
{
	memset(s_field_last, 0, sizeof(s_field_last));
}

uint8_t ui_field(uint8_t id, int x, int y, int w, int h,
                 const char *s, uint16_t fg, uint16_t bg)
{
	char tmp[24];
	int  i, wpx;

	if (id >= UI_FIELD_MAX || s == 0)
		return 0;

	memset(tmp, 0, sizeof(tmp));
	for (i = 0; i < 23 && s[i] != 0; i++)
		tmp[i] = s[i];

	if (strcmp(tmp, s_field_last[id]) == 0)
		return 0;

	memcpy(s_field_last[id], tmp, sizeof(tmp));

	wpx = tw(&Font12, tmp);
	if (wpx > 0) wpx -= 1;

	fillrect(x, y, w, h, bg);
	txt(&Font12, x + (w - wpx) / 2, y + (h - F12H) / 2, tmp, fg, (int32_t)bg);
	return 1;
}

void ui_force_redraw(void)
{
    s_draw_gen++;
	s_chrome_valid = 0;
	ui_field_reset();
}

static const uint8_t s_cn_glyph[5][32] = {
	{ 0x00, 0x00, 0x3F, 0xFC, 0x20, 0x04, 0x21, 0x04, 0x2F, 0xF4, 0x21, 0x04, 0x3F, 0xF4, 0x60, 0x04, 0x67, 0xE4, 0x64, 0x24, 0x64, 0x24, 0x47, 0xE4, 0xC4, 0x04, 0x80, 0x3C, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x66, 0x1C, 0xFF, 0xF0, 0x66, 0xC0, 0x66, 0xC0, 0x7E, 0xC0, 0x66, 0xFE, 0x7E, 0xC8, 0x66, 0xC8, 0x66, 0xC8, 0xFF, 0x88, 0x24, 0x88, 0x67, 0x88, 0xC1, 0x08, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x44, 0x44, 0x64, 0x44, 0x37, 0x54, 0x05, 0x54, 0xC5, 0x64, 0x64, 0x44, 0x07, 0xFC, 0x04, 0xC4, 0x2C, 0xE4, 0x2D, 0x74, 0x6B, 0x54, 0x4A, 0x44, 0x58, 0x44, 0x50, 0x44, 0x00, 0x00 },
	{ 0x00, 0x00, 0x13, 0xFC, 0x10, 0x18, 0xFC, 0x60, 0x30, 0xC0, 0x33, 0x00, 0x7F, 0xFC, 0x7D, 0xA4, 0xD5, 0x24, 0x92, 0x64, 0x16, 0x44, 0x18, 0x84, 0x11, 0x8C, 0x12, 0x38, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x08, 0x78, 0x7F, 0x48, 0x22, 0x48, 0x14, 0x48, 0x7F, 0x8E, 0x18, 0x00, 0x39, 0xFC, 0xDE, 0x4C, 0x24, 0x48, 0x4E, 0x38, 0x35, 0x30, 0x44, 0x7C, 0x39, 0x86, 0x00, 0x00, 0x00, 0x00 },
};

static void cn_glyph(int x, int y, const uint8_t *g, uint16_t color)
{
	int r, c;

	for (r = 0; r < 16; r++) {
		for (c = 0; c < 16; c++) {
			uint8_t b = g[r * 2 + (c >> 3)];
			if ((b & (0x80 >> (c & 7))) != 0)
				px(x + c, y + r, color);
		}
	}
}

void ui_splash(void)
{
	int i;

	fillrect(0, 0, SCR_W, SCR_H, C_BG);

	txt(&Font12, 180, 112, "23211312 ", C_WHITE, (int32_t)C_BG);
	for (i = 0; i < 3; i++)
		cn_glyph(252 + i * 16, 108, s_cn_glyph[i], C_WHITE);

	txt(&Font12, 188, 152, "23211224 ", C_WHITE, (int32_t)C_BG);
	for (i = 0; i < 2; i++)
		cn_glyph(260 + i * 16, 148, s_cn_glyph[3 + i], C_WHITE);

	flip();
}
