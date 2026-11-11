/* app_link.c：链路层：串口调度、命令下发、在线统计 */

#include "main.h"
#include "app_link.h"
#include "app_proto.h"
#include "app_gfx.h"
#include "drv_uart.h"
#include <string.h>
#include <stdio.h>

#define RX_CHUNK        512
#define CMD_BUF_SIZE    64
#define STATE_POLL_MS   200u

static uint8_t s_trigger_pending;
static char s_trigger_ack[48];
static wave_window_t s_win;
static uint8_t       s_win_valid;
static uint8_t       s_received;
static uint32_t      s_last_rx_ms;
static uint32_t      s_last_sync_ms;

static uint32_t      s_frames;
static uint32_t      s_fps_win_ms;
static uint32_t      s_fps_cnt;

static uint8_t       s_rx[RX_CHUNK];
static char          s_cmd[CMD_BUF_SIZE];

static void publish_stats(void)
{
    g_ui.link_ok = app_link_is_alive();
    g_ui.frame_count = s_frames;
    g_ui.lost_frames       = proto_get_stats()->lost_frames;
    g_ui.bad_frames     = proto_get_stats()->bad_frames;
}

void app_link_init(void)
{
    memset(&s_win, 0, sizeof(s_win));
    s_win_valid  = 0;
    s_received = 0;
    s_last_rx_ms = 0;
    s_last_sync_ms = HAL_GetTick();
    s_frames     = 0;
    s_fps_win_ms = HAL_GetTick();
    s_fps_cnt    = 0;

    proto_init();
    s_trigger_pending = 0;

    g_ui.frame_count = 0;
    g_ui.lost_frames       = 0;
    g_ui.bad_frames     = 0;
    g_ui.fps_x10     = 0;

    app_link_cmd_sync();
}

static void handle_text_line(const char *line)
{
    if (line == NULL || line[0] == '\0') return;
    if (strncmp(line, "$OK:", 4) == 0 || strncmp(line, "$ERR:", 5) == 0 ||
        (strncmp(line, "$STATE", 6) == 0 && proto_get_remote_state()->valid)) {
        s_received = 1;
        s_last_rx_ms = HAL_GetTick();
    }
    if (strcmp(line, s_trigger_ack) == 0 || strncmp(line, "$ERR:", 5) == 0)
        s_trigger_pending = 0;
    if (s_trigger_pending && strncmp(line, "$STATE", 6) == 0) return;

    unsigned int tb;
    if (sscanf(line, "[KEY] Switch Timebase to [%u]", &tb) == 1 && tb < SCOPE_TIMEBASE_COUNT) {
        g_ui.scan = (uint8_t)tb;
        ui_force_redraw();
    }

    else if (strncmp(line, "$STATE", 6) == 0) {
        const proto_state_t *st = proto_get_remote_state();
        if (st && st->valid) {
            g_ui.scan       = st->timebase;
            g_ui.run        = st->run;
            g_ui.sqw_freq   = st->gen_freq;
            g_ui.trig_src   = st->trig_src;
            g_ui.trig_edge  = st->trig_edge;
            g_ui.trig_mode  = st->trig_mode;
            g_ui.trig_level = (uint16_t)st->trig_level * 16;
            ui_force_redraw();
        }
    }

    else if (strncmp(line, "$OK:TB=", 7) == 0) {
        int tb = line[7] - '0';
        if (tb >= 0 && tb <= 4) {
            g_ui.scan = (uint8_t)tb;
            ui_force_redraw();
        }
    }

    else if (strcmp(line, "$OK:RUN") == 0) {
        g_ui.run = 1;
        ui_force_redraw();
    }
    else if (strcmp(line, "$OK:GEN_START") == 0) {
        g_ui.sqw_on = 1;
    }
    else if (strcmp(line, "$OK:GEN_STOP") == 0) {
        g_ui.sqw_on = 0;
    }
    else if (strcmp(line, "$OK:STOP") == 0) {
        g_ui.run = 0;
        ui_force_redraw();
    }
}

uint8_t app_link_poll(void)
{
    uint16_t    n;
    uint32_t    now;
    proto_res_t res;
    uint8_t     got_new_wave = 0;
    uint16_t    guard;

    for (guard = 0; guard < 32u; guard++)
    {
        n = drv_uart_read(s_rx, sizeof(s_rx));
        if (n != 0) proto_feed(s_rx, n);

        for (uint16_t parsed = 0; parsed < 64u; parsed++)
        {
            res = proto_poll();
            if (res == PROTO_RES_NONE) break;

            if (res == PROTO_RES_WAVE)
            {

                s_received = 1;
                s_last_rx_ms = HAL_GetTick();
                if (s_trigger_pending) { proto_get_wave(NULL); continue; }
                if (proto_get_wave(&s_win))
                {
                    g_ui.scan = s_win.scan_code;
                    if (g_ui.trig_mode == SCOPE_TRIG_SINGLE) g_ui.run = 0;
                    s_win_valid  = 1;
                    s_frames++;
                    s_fps_cnt++;
                    got_new_wave = 1;
                }
            }
            else if (res == PROTO_RES_TEXT)
            {
                handle_text_line(proto_get_text());
                got_new_wave = 1;
            }
        }

        if (n == 0) break;
    }

    now = HAL_GetTick();
    if (now - s_last_rx_ms >= STATE_POLL_MS &&
        now - s_last_sync_ms >= STATE_POLL_MS) {
        app_link_cmd_sync();
    }
    if (now - s_fps_win_ms >= 1000u)
    {
        uint32_t fps_x10 = s_fps_cnt * 10000u / (now - s_fps_win_ms);
        if (fps_x10 > 9990u) fps_x10 = 9990u;
        g_ui.fps_x10 = (uint16_t)fps_x10;

        s_fps_cnt    = 0;
        s_fps_win_ms = now;
    }

    publish_stats();
    return got_new_wave;
}

uint8_t app_link_is_alive(void)
{
    return s_received && HAL_GetTick() - s_last_rx_ms < APP_LINK_TIMEOUT_MS;
}

uint8_t app_link_get_window(wave_window_t *w)
{

    if (w == NULL || !s_win_valid) {
        return 0;
    }
    memcpy(w, &s_win, sizeof(*w));
    return 1;
}

static void send_command(const char *command)
{
    drv_uart_send((const uint8_t *)command, (uint16_t)strlen(command));
}

void app_link_cmd_scan(uint8_t scan)
{
    if (scan >= SCOPE_TIMEBASE_COUNT) return;
    snprintf(s_cmd, sizeof(s_cmd), "TB %u\r\n", scan);
    send_command(s_cmd);
}

void app_link_cmd_sqw(uint8_t on, uint32_t freq)
{
    if (on) {
        snprintf(s_cmd, sizeof(s_cmd), "GEN %lu\r\nGEN_START\r\n", (unsigned long)freq);
        send_command(s_cmd);
    } else send_command("GEN_STOP\r\n");
}

void app_link_cmd_trig(uint8_t src, uint8_t edge, uint8_t mode, uint16_t level)
{
    if (src >= SCOPE_TRIGGER_SOURCES || edge > 1 || mode > SCOPE_TRIG_SINGLE || level > 4095) return;
    snprintf(s_trigger_ack, sizeof(s_trigger_ack), "$OK:TRIG=%u,%u,%u,%u",
             src, edge, mode, (unsigned int)(level / 16));
    s_trigger_pending = 1;
    snprintf(s_cmd, sizeof(s_cmd), "TRIG %u %u %u %u\r\nSYNC\r\n",
             src, edge, mode, (unsigned int)(level / 16));
    send_command(s_cmd);
}

void app_link_cmd_run(uint8_t run)
{
    if (run != 0) {
        send_command("RUN\r\n");
    } else {
        send_command("STOP\r\n");
    }
}

void app_link_cmd_arm(void)
{
    strcpy(s_trigger_ack, "$OK:ARM_SINGLE");
    s_trigger_pending = 1;
    g_ui.trig_mode = SCOPE_TRIG_SINGLE;
    g_ui.run = 1;
    send_command("ARM\r\n");
}

void app_link_cmd_sync(void)
{
    s_last_sync_ms = HAL_GetTick();
    send_command("SYNC\r\n");
}
