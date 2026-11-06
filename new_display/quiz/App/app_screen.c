/* 绘制主界面。所有布局坐标都以 480 x 272 屏幕为基准。 */
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
    ui_fill(2, 2, 44, 15, g_ui.run ? C_GREEN : C_RED);
    ui_text_c(24, 3, g_ui.run ? "RUN" : "STOP", C_BLACK, UI_BG_NONE);
    p = ui_str(buf, ui_scan_label(w->scan_code));
    ui_str(p, "/div");
    ui_text(54, 4, buf, C_WHITE, UI_BG_NONE);

    p = ui_str(buf, source[g_ui.trig_src < 5 ? g_ui.trig_src : 1]);
    p = ui_str(p, g_ui.trig_edge ? " v " : " ^ ");
    p = ui_str(p, mode[g_ui.trig_mode < 3 ? g_ui.trig_mode : 0]);
    if (g_ui.trig_src <= TRIGSRC_CH2) {
        p = ui_str(p, " ");
        p = ui_fix(p, (int32_t)g_ui.trig_level * 330 / 4095, 2);
        ui_str(p, "V");
    }
    ui_text(174, 4, buf, C_BLUE, UI_BG_NONE);
    if (g_ui.link_ok) {
        p = ui_fix(buf, g_ui.fps_x10, 1);
        ui_str(p, "fps");
    } else ui_str(buf, g_ui.run ? "NO LINK" : "HOLD");
    ui_text(SCR_W - ui_text_w(buf) - 4, 4, buf, g_ui.link_ok ? C_GREEN : C_AMBER, UI_BG_NONE);
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

void scr_softkeys(void)
{
    int i;
    ui_fill(0, TB_Y + MEAS_H, SCR_W, TB_H - MEAS_H, C_TB_BG);
    for (i = 0; i < SCR_KEYS_N; i++) {
        uint16_t bg = C_TB_BTN, fg = C_WHITE;
        const char *label = s_names[i];
        int x = i * KEY_W + 1;
        if (i == UI_WID_RUN) { fg = g_ui.run ? C_GREEN : C_RED; label = g_ui.run ? "STOP" : "RUN"; }
        else if (i == UI_WID_SCAN) fg = C_BLUE;
        else if (i >= UI_WID_CH1 && i <= UI_WID_DIG2) {
            int channel = i - UI_WID_CH1;
            fg = (g_ui.ch_mask & s_bits[channel]) ? s_colors[channel] : C_DIM;
            bg = (g_ui.ch_mask & s_bits[channel]) ? C_TB_BTN_ON : C_TB_BTN_OFF;
        } else bg = C_TB_MENU;
        ui_fill(x, TB_Y + MEAS_H, KEY_W - 2, TB_H - MEAS_H, bg);
        ui_text_c(x + (KEY_W - 2) / 2, TB_Y + MEAS_H + 7, label, fg, UI_BG_NONE);
    }
}

int scr_softkey_hit(int x, int y)
{
    if (x < 0 || x >= (int)SCR_W || y < (int)TB_Y + MEAS_H || y >= (int)SCR_H)
        return UI_WID_NONE;
    return g_keys[x / KEY_W];
}
