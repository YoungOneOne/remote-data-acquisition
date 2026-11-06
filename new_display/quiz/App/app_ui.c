/* 菜单只管交互，波形绘制在 app_gfx.c，串口命令在 app_link.c。 */
#include "app_ui.h"
#include "app_screen.h"
#include "app_link.h"

const uint8_t g_keys[SCR_KEYS_N] = {
    UI_WID_RUN, UI_WID_SCAN, UI_WID_CH1, UI_WID_CH2,
    UI_WID_DIG0, UI_WID_DIG1, UI_WID_DIG2, UI_WID_MENU
};
static const uint32_t s_presets[] = {100, 500, 1000, 2000, 5000, 10000};
static const char *const s_sources[] = {"CH1", "CH2", "D2", "D0", "D1"};
static const char *const s_modes[] = {"AUTO", "NORMAL", "SINGLE"};
static ui_btn_t s_back = UI_BTN_DEFAULT;
static ui_btn_t s_tabs[3];
static ui_btn_t s_buttons[7];
static uint8_t s_page_changed = 1;
static ui_state_t s_last_settings;
static wave_window_t s_measure_window;

static void push_trigger(void)
{
    app_link_cmd_trig(g_ui.trig_src, g_ui.trig_edge, g_ui.trig_mode, g_ui.trig_level);
}

/* 状态回复到达后刷新设置；普通波形到达不会让菜单跟着重画。 */
uint8_t app_ui_needs_refresh(void)
{
    uint8_t changed =
        g_ui.trig_src != s_last_settings.trig_src ||
        g_ui.trig_mode != s_last_settings.trig_mode ||
        g_ui.trig_edge != s_last_settings.trig_edge ||
        g_ui.trig_level != s_last_settings.trig_level ||
        g_ui.sqw_freq != s_last_settings.sqw_freq ||
        g_ui.sqw_on != s_last_settings.sqw_on ||
        g_ui.run != s_last_settings.run;
    s_last_settings = g_ui;
    return changed;
}

static void settings_reset(void)
{
    int i;
    s_page_changed = 1;
    for (i = 0; i < 7; i++) ui_btn_invalidate(&s_buttons[i]);
    ui_field_reset();
}

static void row_label(int y, const char *label)
{
    ui_text(18, y + 8, label, C_DIM, UI_BG_NONE);
}

static void trigger_settings(void)
{
    char buf[24], *p;
    row_label(64, "Source");
    if (ui_button(&s_buttons[0], 178, 64, 280, 30, s_sources[g_ui.trig_src < 5 ? g_ui.trig_src : 1], 0)) {
        g_ui.trig_src = (g_ui.trig_src + 1) % 5;
        push_trigger();
    }
    row_label(102, "Mode");
    if (ui_button(&s_buttons[1], 178, 102, 280, 30, s_modes[g_ui.trig_mode < 3 ? g_ui.trig_mode : 0], 0)) {
        g_ui.trig_mode = (g_ui.trig_mode + 1) % 3;
        push_trigger();
    }
    row_label(140, "Edge");
    if (ui_button(&s_buttons[2], 178, 140, 280, 30, g_ui.trig_edge ? "FALLING" : "RISING", 0)) {
        g_ui.trig_edge ^= 1;
        push_trigger();
    }
    row_label(178, "Level");
    p = ui_fix(buf, (int32_t)g_ui.trig_level * 3300 / 4095, 3);
    ui_str(p, " V");
    ui_field(0, 238, 178, 160, 30, g_ui.trig_src > 1 ? "Digital edge" : buf, C_BLUE, C_BG);
    if (ui_button(&s_buttons[3], 178, 178, 52, 30, "-", 0) && g_ui.trig_src <= 1) {
        g_ui.trig_level = g_ui.trig_level >= 128 ? g_ui.trig_level - 128 : 0;
        push_trigger();
    }
    if (ui_button(&s_buttons[4], 406, 178, 52, 30, "+", 0) && g_ui.trig_src <= 1) {
        g_ui.trig_level = g_ui.trig_level <= 3952 ? g_ui.trig_level + 128 : 4080;
        push_trigger();
    }
    if (ui_button(&s_buttons[5], 18, 228, 210, 32, "ARM SINGLE", 0)) app_link_cmd_arm();
    if (ui_button(&s_buttons[6], 246, 228, 212, 32, "SYNC STATE", 0)) app_link_cmd_sync();
}

static void change_frequency(int direction)
{
    int i, index = 0;
    for (i = 0; i < 6; i++) if (g_ui.sqw_freq >= s_presets[i]) index = i;
    index = (index + direction + 6) % 6;
    g_ui.sqw_freq = s_presets[index];
    app_link_cmd_sqw(g_ui.sqw_on, g_ui.sqw_freq);
}

static void output_settings(void)
{
    char buf[24], *p;
    row_label(74, "Square wave");
    if (ui_button(&s_buttons[0], 234, 74, 224, 36, g_ui.sqw_on ? "OUTPUT ON" : "OUTPUT OFF", g_ui.sqw_on)) {
        g_ui.sqw_on ^= 1;
        app_link_cmd_sqw(g_ui.sqw_on, g_ui.sqw_freq);
    }
    row_label(130, "Frequency");
    p = ui_itoa(buf, g_ui.sqw_freq);
    ui_str(p, " Hz");
    ui_field(1, 240, 130, 160, 36, buf, C_BLUE, C_BG);
    if (ui_button(&s_buttons[1], 178, 130, 52, 36, "-", 0)) change_frequency(-1);
    if (ui_button(&s_buttons[2], 406, 130, 52, 36, "+", 0)) change_frequency(1);
    ui_text(18, 190, "100 / 500 / 1k / 2k / 5k / 10k Hz", C_DIM, UI_BG_NONE);
    ui_text(18, 212, "PB4 output  |  Duty 50%", C_DIM, UI_BG_NONE);
    ui_text(18, 244, "Output switch does not stop acquisition.", C_DIM, UI_BG_NONE);
}

static void measurement_settings(void)
{
    ui_meas_all_t m;
    const ui_meas_t *values[5] = {&m.ch1, &m.ch2, &m.dig0, &m.dig1, &m.dig2};
    const char *const names[5] = {"CH1", "CH2", "D0", "D1", "D2"};
    const uint16_t colors[5] = {C_CH1, C_CH2, C_DIG0, C_DIG1, C_DIG2};
    char buf[32], *p;
    int i, y;
    /* 一次取同一帧，所有读数都基于该帧；点 REFRESH 更新。 */
    ui_measure(&s_measure_window, &m);
    ui_text(18, 68, "Channel    Frequency      Vpp / Duty", C_DIM, UI_BG_NONE);
    for (i = 0; i < 5; i++) {
        y = 94 + i * 24;
        ui_text(18, y, names[i], colors[i], UI_BG_NONE);
        p = buf;
        if (values[i]->valid && values[i]->freq > 0) {
            p = ui_itoa(p, (int32_t)(values[i]->freq + 0.5f));
            ui_str(p, " Hz");
        } else ui_str(p, "--");
        ui_field(i, 120, y - 2, 150, 20, buf, colors[i], C_BG);
        p = ui_fix(buf, (int32_t)((i < 2 ? values[i]->vpp * 100 : values[i]->duty * 100) + 0.5f), i < 2 ? 2 : 0);
        ui_str(p, i < 2 ? " V" : " %");
        ui_text_at(292, y - 2, 166, 20, buf, colors[i], C_BG);
    }
    if (ui_button(&s_buttons[0], 18, 228, 210, 32, "REFRESH", 0)) {
        app_link_get_window(&s_measure_window);
        settings_reset();
    }
    p = ui_str(buf, "Drop "); p = ui_itoa(p, g_ui.lost_frames);
    p = ui_str(p, "  Err "); ui_itoa(p, g_ui.bad_frames);
    ui_text_at(246, 228, 212, 32, buf, C_DIM, C_BG);
}

void app_ui_draw_settings(void)
{
    static const char *const tabs[] = {"TRIGGER", "OUTPUT", "MEASURE"};
    int i;
    if (ui_page_get() != UI_PAGE_SETTINGS) return;
    if (s_page_changed) {
        ui_fill(0, 60, SCR_W, SCR_H - 60, C_BG);
        s_page_changed = 0;
    }
    ui_fill(0, 0, 380, 26, C_ST_BG);
    ui_text(12, 7, "SETTINGS", C_WHITE, UI_BG_NONE);
    if (ui_button(&s_back, 386, 2, 84, 22, "BACK", 0)) {
        ui_page_set(UI_PAGE_WAVE);
        return;
    }
    for (i = 0; i < 3; i++) {
        if (ui_button(&s_tabs[i], 6 + i * 156, 30, 152, 28, tabs[i], g_ui.tab == i)) {
            g_ui.tab = i;
            settings_reset();
            ui_force_redraw();
            return;
        }
    }
    if (g_ui.tab == TAB_TRIG) trigger_settings();
    else if (g_ui.tab == TAB_SQW) output_settings();
    else if (g_ui.tab == TAB_MEAS) measurement_settings();
}

void app_ui_init(void)
{
    int i;
    g_ui.scan = 2; /* 与采集端默认的 1ms/div 一致。 */
    g_ui.ch_mask = CHM_ANA | CHM_DIG;
    g_ui.run = 1;
    g_ui.tab = TAB_TRIG;
    g_ui.trig_src = TRIGSRC_CH2;
    g_ui.trig_mode = 0;
    g_ui.trig_edge = 0;
    g_ui.trig_level = 2048;
    g_ui.sqw_on = 1;
    g_ui.sqw_freq = 1000;
    for (i = 0; i < 3; i++) ui_btn_init(&s_tabs[i]);
    for (i = 0; i < 7; i++) ui_btn_init(&s_buttons[i]);
    ui_force_redraw();
}

void app_ui_widget(int wid)
{
    if (wid == UI_WID_RUN) {
        g_ui.run ^= 1;
        app_link_cmd_run(g_ui.run);
    } else if (wid == UI_WID_SCAN) {
        g_ui.scan = (g_ui.scan + 1) % 5;
        app_link_cmd_scan(g_ui.scan);
    } else if (wid >= UI_WID_CH1 && wid <= UI_WID_DIG2) {
        g_ui.ch_mask ^= (uint8_t)(1u << (wid - UI_WID_CH1));
    } else if (wid == UI_WID_MENU) {
        app_link_get_window(&s_measure_window);
        settings_reset();
        ui_page_set(UI_PAGE_SETTINGS);
        app_link_cmd_sync();
    }
    ui_force_redraw();
}
