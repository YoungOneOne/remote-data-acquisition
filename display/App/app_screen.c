/* app_screen.c：屏幕分区：状态栏、测量行、底栏软键 */

#include "app_screen.h"

#define MEAS_H 18
#define KEY_W (SCR_W / SCR_KEYS_N)

static const char *const s_names[] = {"RUN", "TIME", "CH1", "CH2", "D0", "D1", "D2", "MENU"};
static const uint8_t s_bits[] = {CHM_CH1, CHM_CH2, CHM_DIG0, CHM_DIG1, CHM_DIG2};
static const uint16_t s_colors[] = {C_CH1, C_CH2, C_DIG0, C_DIG1, C_DIG2};

void scr_status_bar(const wave_window_t *w)
{
    char buf[40], *p;
    static const char *const source[] = {"CH1", "CH2", "D2", "D0", "D1"};
    static const char *const mode[] = {"AUTO", "NORM", "SINGLE"};
    ui_fill(0, ST_Y, SCR_W, ST_H, C_ST_BG);
    if (g_ui.run != 0) {
        ui_fill(2, 2, 44, 15, C_GREEN);
        ui_text_c(24, 3, "RUN", C_BLACK, UI_BG_NONE);
    } else {
        ui_fill(2, 2, 44, 15, C_RED);
        ui_text_c(24, 3, "STOP", C_BLACK, UI_BG_NONE);
    }
    p = ui_str(buf, ui_scan_label(w->scan_code));
    ui_str(p, "/div");
    ui_text(54, 4, buf, C_WHITE, UI_BG_NONE);

    if (g_ui.trig_src < 5) {
        p = ui_str(buf, source[g_ui.trig_src]);
    } else {
        p = ui_str(buf, source[1]);
    }
    if (g_ui.trig_edge != 0) {
        p = ui_str(p, " v ");
    } else {
        p = ui_str(p, " ^ ");
    }
    if (g_ui.trig_mode < 3) {
        p = ui_str(p, mode[g_ui.trig_mode]);
    } else {
        p = ui_str(p, mode[0]);
    }
    if (g_ui.trig_src <= TRIGSRC_CH2) {
        p = ui_str(p, " ");
        p = ui_fix(p, (int32_t)g_ui.trig_level * 330 / 4095, 2);
        ui_str(p, "V");
    }
    ui_text(174, 4, buf, C_BLUE, UI_BG_NONE);
    if (!g_ui.link_ok) {
        ui_str(buf, "NO LINK");
    } else if (!g_ui.run) {
        ui_str(buf, "HOLD");
    } else if (g_ui.trig_mode != SCOPE_TRIG_AUTO && g_ui.fps_x10 == 0) {
        ui_str(buf, "WAIT");
    } else {
        p = ui_fix(buf, g_ui.fps_x10, 1);
        ui_str(p, "fps");
    }
    if (g_ui.link_ok != 0) {
        ui_text(SCR_W - ui_text_w(buf) - 4, 4, buf, C_GREEN, UI_BG_NONE);
    } else {
        ui_text(SCR_W - ui_text_w(buf) - 4, 4, buf, C_AMBER, UI_BG_NONE);
    }
    ui_hline(0, SCR_W - 1, ST_H - 1, C_EDGE);
}

void scr_meas_line(const wave_window_t *w)
{
    ui_meas_all_t m;
    char buf[32], *p;
    ui_fill(0, TB_Y, SCR_W, MEAS_H, C_TB_BG);
    if (!w || w->n < 2) return;
    ui_measure(w, &m);
    p = ui_str(buf, "CH1 ");
    p = ui_fix(p, (int32_t)(m.ch1.vavg * 100 + 0.5f), 2);
    ui_str(p, "V");
    ui_text(4, TB_Y + 3, buf, C_CH1, UI_BG_NONE);
    p = ui_str(buf, "CH2 ");
    if (m.ch2.valid && m.ch2.freq > 0) {
        p = ui_itoa(p, (int32_t)(m.ch2.freq + 0.5f));
        ui_str(p, "Hz");
    } else ui_str(p, "--Hz");
    ui_text(130, TB_Y + 3, buf, C_CH2, UI_BG_NONE);
    p = ui_str(buf, "Vpp ");
    p = ui_fix(p, (int32_t)(m.ch2.vpp * 100 + 0.5f), 2);
    ui_str(p, "V");
    ui_text(278, TB_Y + 3, buf, C_CH2, UI_BG_NONE);
    p = ui_str(buf, "D ");
    p = ui_itoa(p, w->dig[w->n - 1] & 1);
    p = ui_itoa(p, (w->dig[w->n - 1] >> 1) & 1);
    ui_itoa(p, (w->dig[w->n - 1] >> 2) & 1);
    ui_text(416, TB_Y + 3, buf, C_DIM, UI_BG_NONE);
}

static void sk_0(void) { ui_softkey_dispatch(0); }
static void sk_1(void) { ui_softkey_dispatch(1); }
static void sk_2(void) { ui_softkey_dispatch(2); }
static void sk_3(void) { ui_softkey_dispatch(3); }
static void sk_4(void) { ui_softkey_dispatch(4); }
static void sk_5(void) { ui_softkey_dispatch(5); }
static void sk_6(void) { ui_softkey_dispatch(6); }
static void sk_7(void) { ui_softkey_dispatch(7); }

static void (*const s_sk_act[SCR_KEYS_N])(void) = {
	sk_0, sk_1, sk_2, sk_3, sk_4, sk_5, sk_6, sk_7
};

void scr_softkeys(void)
{
	int i;

	for (i = 0; i < SCR_KEYS_N; i++) {
		uint16_t bg    = C_TB_BTN,      fg    = C_WHITE;
		uint16_t bg_on = C_TB_BTN_ON,   fg_on = C_GREEN;
		const char *label = s_names[i];
		int on = 0;
		int x  = i * KEY_W + 1;

		if (i == UI_WID_RUN) {
			if (g_ui.run != 0) {
				label = "STOP";
				fg    = C_GREEN;
				on    = 1;
			} else {
				label = "RUN";
				fg    = C_RED;
				on    = 0;
			}
		} else if (i == UI_WID_SCAN) {
			fg    = C_BLUE;
			fg_on = C_BLUE;
		} else if (i >= UI_WID_CH1 && i <= UI_WID_DIG2) {
			int channel = i - UI_WID_CH1;
			if ((g_ui.ch_mask & s_bits[channel]) != 0) {
				on    = 1;
				fg_on = s_colors[channel];
				bg_on = C_TB_BTN_ON;
			} else {
				fg = C_DIM;
				bg = C_TB_BTN_OFF;
				fg_on = C_DIM;
				bg_on = C_TB_BTN_OFF;
			}
		} else {
			bg    = C_TB_MENU;
			bg_on = C_TB_MENU;
			fg_on = C_WHITE;
		}

		ui_action_button_color(24 + i, x, TB_Y + MEAS_H,
		                       KEY_W - 2, TB_H - MEAS_H,
		                       label, on, bg, fg, bg_on, fg_on,
		                       s_sk_act[i]);
	}
}

void scr_toolbar(const wave_window_t *w)
{
    ui_fill(0, TB_Y, SCR_W, TB_H, C_TB_BG);
    scr_softkeys();
    scr_meas_line(w);
}

int scr_softkey_hit(int x, int y)
{
    if (x < 0 || x >= (int)SCR_W || y < (int)TB_Y + MEAS_H || y >= (int)SCR_H)
        return UI_WID_NONE;
    return g_keys[x / KEY_W];
}
