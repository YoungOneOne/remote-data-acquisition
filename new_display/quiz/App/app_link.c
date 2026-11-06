/**
  ******************************************************************************
  * @file    app_link.c
  * @brief   与采集端的链路管理实现
  ******************************************************************************
  */

#include "main.h"
#include "app_link.h"
#include "app_proto.h"
#include "app_gfx.h"
#include "drv_uart.h"
#include <string.h>
#include <stdio.h>

#define RX_CHUNK        512         /* 每次从驱动队列搬运的字节数 */
#define CMD_BUF_SIZE    64

static uint8_t s_trigger_pending;
static char s_trigger_ack[48];
static wave_window_t s_win;         /* 最近一窗有效数据 */
static uint8_t       s_win_valid;   /* 是否收到过波形数据 */
static uint32_t      s_last_wave_ms;  /* 最近收到数据的时刻 */

/* 统计 */
static uint32_t      s_frames;      /* 收到的波形帧数 */
static uint32_t      s_fps_win_ms;  /* fps 统计窗口起点 */
static uint32_t      s_fps_cnt;     /* 窗口内帧数 */

static uint8_t       s_rx[RX_CHUNK];
static char          s_cmd[CMD_BUF_SIZE];

/* 状态栏统计刷新 */
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
    s_last_wave_ms = 0;
    s_frames     = 0;
    s_fps_win_ms = HAL_GetTick();
    s_fps_cnt    = 0;

    proto_init();
    s_trigger_pending = 0;

    g_ui.frame_count = 0;
    g_ui.lost_frames       = 0;
    g_ui.bad_frames     = 0;
    g_ui.fps_x10     = 0;

    /* 上电向采集端发送一次 SYNC 同步指令 */
    app_link_cmd_sync();
}

/**
  * @brief  处理从串口提取出的 ASCII 文本行 ($OK, $STATE, [KEY] 等)
  */
static void handle_text_line(const char *line)
{
    if (line == NULL || line[0] == '\0') return;
    if (strcmp(line, s_trigger_ack) == 0 || strncmp(line, "$ERR:", 5) == 0)
        s_trigger_pending = 0;
    if (s_trigger_pending && strncmp(line, "$STATE", 6) == 0) return;

    /* 采集板本地按键发出的实际通知格式。 */
    unsigned int tb;
    if (sscanf(line, "[KEY] Switch Timebase to [%u]", &tb) == 1 && tb < SCOPE_TIMEBASE_COUNT) {
        g_ui.scan = (uint8_t)tb;
        ui_force_redraw();
    }
    /* 2. 全局状态同步应答: $STATE,TB:2,MODE:1,GEN:1000,RUN:1,TRIG:CH2,RISING,AUTO,LVL:128,TRGD:1 */
    else if (strncmp(line, "$STATE", 6) == 0) {
        const proto_state_t *st = proto_get_remote_state();
        if (st && st->valid) {
            g_ui.scan       = st->timebase;
            g_ui.run        = st->run;
            g_ui.sqw_freq   = st->gen_freq;
            g_ui.trig_src   = st->trig_src;
            g_ui.trig_edge  = st->trig_edge;
            g_ui.trig_mode  = st->trig_mode;
            g_ui.trig_level = (uint16_t)st->trig_level << 4;
            ui_force_redraw();
        }
    }
    /* 3. 成功确认反馈: $OK:TB=3 */
    else if (strncmp(line, "$OK:TB=", 7) == 0) {
        int tb = line[7] - '0';
        if (tb >= 0 && tb <= 4) {
            g_ui.scan = (uint8_t)tb;
            ui_force_redraw();
        }
    }
    /* 4. 成功确认反馈: $OK:RUN / $OK:STOP */
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

    /* Parse after each DMA chunk; don't overflow the protocol stream with a backlog. */
    for (guard = 0; guard < 32u; guard++)
    {
        n = drv_uart_read(s_rx, sizeof(s_rx));
        if (n != 0) proto_feed(s_rx, n);
        /* 2. 循环解析可能积压的完整包（同样限轮数，防止解析层不消费导致死循环） */
        for (uint16_t parsed = 0; parsed < 64u; parsed++)
        {
            res = proto_poll();
            if (res == PROTO_RES_NONE) break;

            if (res == PROTO_RES_WAVE)
            {
                if (s_trigger_pending) { proto_get_wave(NULL); continue; }
                if (proto_get_wave(&s_win))
                {
                    g_ui.scan = s_win.scan_code;
                    if (g_ui.trig_mode == SCOPE_TRIG_SINGLE) g_ui.run = 0;
                    s_win_valid  = 1;
                    s_last_wave_ms = HAL_GetTick();
                    s_frames++;
                    s_fps_cnt++;
                    got_new_wave = 1;
                }
            }
            else if (res == PROTO_RES_TEXT)
            {
                handle_text_line(proto_get_text());
                got_new_wave = 1; /* Control ACK also needs a UI refresh, including STOP. */
            }
        }

        if (n == 0) break;
    }

    /* 3. 统计实际帧率 (每 1 秒计算一次) */
    now = HAL_GetTick();
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
    if (!s_win_valid) {
        return 0;
    }
    return (HAL_GetTick() - s_last_wave_ms < APP_LINK_TIMEOUT_MS) ? 1u : 0u;
}

uint8_t app_link_get_window(wave_window_t *w)
{
    /* 只要收到过帧就返回最近一帧，**不再因为超时返回 0**。
     * 原来这里跟着 is_alive() 一起判超时，一旦采集端暂停或线松了，
     * app_task 里的 ui_draw 就再也不会被调用 —— 触摸、菜单全部没反应，
     * 看起来就是"程序卡死"。屏幕定格在最后一帧才是示波器该有的行为，
     * 在不在线由状态栏的 NO SIG 提示。 */
    if (w == NULL || !s_win_valid) {
        return 0;
    }
    memcpy(w, &s_win, sizeof(*w));
    return 1;
}

/* 简单命令直接发送；带参数的命令先写入同一个小缓冲区。 */
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
             src, edge, mode, (unsigned int)(level >> 4));
    s_trigger_pending = 1;
    snprintf(s_cmd, sizeof(s_cmd), "TRIG %u %u %u %u\r\nSYNC\r\n",
             src, edge, mode, (unsigned int)(level >> 4));
    send_command(s_cmd);
}

void app_link_cmd_run(uint8_t run)
{
    send_command(run ? "RUN\r\n" : "STOP\r\n");
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
    send_command("SYNC\r\n");
}
