/* app_ui.c：界面交互：设置页、按键分发 */

#include "app_ui.h"
#include "app_screen.h"
#include "ui_kit.h"
#include "app_link.h"

const uint8_t g_keys[SCR_KEYS_N] = {
    UI_WID_RUN, UI_WID_SCAN, UI_WID_CH1, UI_WID_CH2,
    UI_WID_DIG0, UI_WID_DIG1, UI_WID_DIG2, UI_WID_MENU
};
static const uint32_t s_presets[] = {100, 500, 1000, 2000, 5000, 10000};
static const char *const s_sources[] = {"CH1", "CH2", "D2", "D0", "D1"};
static const char *const s_modes[] = {"AUTO", "NORMAL", "SINGLE"};

static uint8_t s_page_changed = 1;
static wave_window_t s_measure_window;

static void push_trigger(void)
{
    app_link_cmd_trig(g_ui.trig_src, g_ui.trig_edge, g_ui.trig_mode, g_ui.trig_level);
}

static void settings_reset(void)
{

    s_page_changed = 1;

    ui_field_reset();
}

static void row_label(int y, const char *label)
{
    kit_title(y, label);
}

static void trigger_settings(void)
{
    char buf[24], *p;
    const char *label;
    int step;

    if (g_ui.trig_src < 5) {
        label = s_sources[g_ui.trig_src];
    } else {
        label = s_sources[1];
    }
    if (kit_button(0, 0, "Source", label, 0)) {
        g_ui.trig_src = (g_ui.trig_src + 1) % 5;
        push_trigger();
    }

    if (g_ui.trig_mode < 3) {
        label = s_modes[g_ui.trig_mode];
    } else {
        label = s_modes[0];
    }
    if (kit_button(1, 1, "Mode", label, 0)) {
        g_ui.trig_mode = (g_ui.trig_mode + 1) % 3;
        push_trigger();
    }

    if (g_ui.trig_edge != 0) {
        label = "FALLING";
    } else {
        label = "RISING";
    }
    if (kit_button(2, 2, "Edge", label, 0)) {
        if (g_ui.trig_edge == 0) {
            g_ui.trig_edge = 1;
        } else {
            g_ui.trig_edge = 0;
        }
        push_trigger();
    }

    if (g_ui.trig_src > 1) {
        p = ui_str(buf, "Digital edge");
    } else {
        p = ui_fix(buf, (int32_t)g_ui.trig_level * 3300 / 4095, 3);
        ui_str(p, " V");
    }
    step = kit_step(3, 3, "Level", buf, "-", "+");
    if (step != 0 && g_ui.trig_src <= 1) {
        if (step < 0) {
            if (g_ui.trig_level >= 128) {
                g_ui.trig_level = g_ui.trig_level - 128;
            } else {
                g_ui.trig_level = 0;
            }
        } else {
            if (g_ui.trig_level <= 3952) {
                g_ui.trig_level = g_ui.trig_level + 128;
            } else {
                g_ui.trig_level = 4080;
            }
        }
        push_trigger();
    }

    if (ui_action_button(5, 18, 228, 210, 32, "ARM SINGLE", 0, 0)) app_link_cmd_arm();
    if (ui_action_button(6, 246, 228, 212, 32, "SYNC STATE", 0, 0)) app_link_cmd_sync();
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
    const char *label;
    row_label(74, "Square wave");
    if (g_ui.sqw_on != 0) {
        label = "OUTPUT ON";
    } else {
        label = "OUTPUT OFF";
    }
    if (ui_action_button(7, 234, 74, 224, 36, label, g_ui.sqw_on, 0)) {
        if (g_ui.sqw_on == 0) {
            g_ui.sqw_on = 1;
        } else {
            g_ui.sqw_on = 0;
        }
        app_link_cmd_sqw(g_ui.sqw_on, g_ui.sqw_freq);
    }
    row_label(130, "Frequency");
    p = ui_itoa(buf, g_ui.sqw_freq);
    ui_str(p, " Hz");
    ui_field(1, 240, 130, 160, 36, buf, C_BLUE, C_BG);
    if (ui_action_button(8, 178, 130, 52, 36, "-", 0, 0)) change_frequency(-1);
    if (ui_action_button(9, 406, 130, 52, 36, "+", 0, 0)) change_frequency(1);
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

    ui_measure(&s_measure_window, &m);

    ui_text   (18,  68, "Channel",    C_DIM, UI_BG_NONE);
    ui_text_at(120, 68, 150, 20,      "Frequency", C_DIM, C_BG);
    ui_text_at(292, 68, 166, 20,      "Vpp / Duty", C_DIM, C_BG);
    for (i = 0; i < 5; i++) {
        y = 94 + i * 24;
        ui_text(18, y, names[i], colors[i], UI_BG_NONE);
        p = buf;
        if (values[i]->valid && values[i]->freq > 0) {
            p = ui_itoa(p, (int32_t)(values[i]->freq + 0.5f));
            ui_str(p, " Hz");
        } else ui_str(p, "--");
        ui_field(i, 120, y - 2, 150, 20, buf, colors[i], C_BG);
        if (i < 2) {
            p = ui_fix(buf, (int32_t)(values[i]->vpp * 100 + 0.5f), 2);
            ui_str(p, " V");
        } else {
            p = ui_fix(buf, (int32_t)(values[i]->duty * 100 + 0.5f), 0);
            ui_str(p, " %");
        }
        ui_text_at(292, y - 2, 166, 20, buf, colors[i], C_BG);
    }
    if (ui_action_button(10, 18, 228, 210, 32, "REFRESH", 0, 0)) {
        app_link_get_window(&s_measure_window);
        settings_reset();
    }
    p = ui_str(buf, "Drop "); p = ui_itoa(p, g_ui.lost_frames);
    p = ui_str(p, "  Err "); ui_itoa(p, g_ui.bad_frames);
    ui_text_at(246, 228, 212, 32, buf, C_DIM, C_BG);
}

static void settings_draw(void)
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
    if (ui_action_button(19, 386, 2, 84, 22, "BACK", 0, 0)) {
        ui_page_set(UI_PAGE_WAVE);
        return;
    }
    for (i = 0; i < 3; i++) {
        if (ui_action_button(20 + i, 6 + i * 118, 30, 114, 28, tabs[i], g_ui.tab == i, 0)) {
            g_ui.tab = i;
            settings_reset();
            ui_force_redraw();
            return;
        }
    }
    if (g_ui.tab == TAB_TRIG) trigger_settings();
    else if (g_ui.tab == TAB_SQW) output_settings();
    else                                 measurement_settings();
}

void app_ui_init(void)
{
    int i;
    g_ui.scan = 2;
    g_ui.ch_mask = CHM_ANA | CHM_DIG;
    g_ui.run = 1;
    g_ui.trig_src = TRIGSRC_CH2;
    g_ui.trig_mode = 0;
    g_ui.trig_edge = 0;
    g_ui.trig_level = 2048;
    g_ui.sqw_on = 1;
    g_ui.sqw_freq = 1000;

    ui_set_custom(settings_draw);
    ui_set_softkey_handler(app_ui_widget);
    ui_force_redraw();
}

void app_ui_widget(int wid)
{
    if (wid == UI_WID_RUN) {
        if (g_ui.run == 0) {
            g_ui.run = 1;
        } else {
            g_ui.run = 0;
        }
        app_link_cmd_run(g_ui.run);
    } else if (wid == UI_WID_SCAN) {
        g_ui.scan = (g_ui.scan + 1) % 5;
        app_link_cmd_scan(g_ui.scan);
    } else if (wid == UI_WID_CH1) {
        if ((g_ui.ch_mask & CHM_CH1) != 0) {
            g_ui.ch_mask = (uint8_t)(g_ui.ch_mask & ~CHM_CH1);
        } else {
            g_ui.ch_mask = (uint8_t)(g_ui.ch_mask | CHM_CH1);
        }
    } else if (wid == UI_WID_CH2) {
        if ((g_ui.ch_mask & CHM_CH2) != 0) {
            g_ui.ch_mask = (uint8_t)(g_ui.ch_mask & ~CHM_CH2);
        } else {
            g_ui.ch_mask = (uint8_t)(g_ui.ch_mask | CHM_CH2);
        }
    } else if (wid == UI_WID_DIG0) {
        if ((g_ui.ch_mask & CHM_DIG0) != 0) {
            g_ui.ch_mask = (uint8_t)(g_ui.ch_mask & ~CHM_DIG0);
        } else {
            g_ui.ch_mask = (uint8_t)(g_ui.ch_mask | CHM_DIG0);
        }
    } else if (wid == UI_WID_DIG1) {
        if ((g_ui.ch_mask & CHM_DIG1) != 0) {
            g_ui.ch_mask = (uint8_t)(g_ui.ch_mask & ~CHM_DIG1);
        } else {
            g_ui.ch_mask = (uint8_t)(g_ui.ch_mask | CHM_DIG1);
        }
    } else if (wid == UI_WID_DIG2) {
        if ((g_ui.ch_mask & CHM_DIG2) != 0) {
            g_ui.ch_mask = (uint8_t)(g_ui.ch_mask & ~CHM_DIG2);
        } else {
            g_ui.ch_mask = (uint8_t)(g_ui.ch_mask | CHM_DIG2);
        }
    } else if (wid == UI_WID_MENU) {
        app_link_get_window(&s_measure_window);
        settings_reset();
        ui_page_set(UI_PAGE_SETTINGS);
        app_link_cmd_sync();
    }
    ui_force_redraw();
}
