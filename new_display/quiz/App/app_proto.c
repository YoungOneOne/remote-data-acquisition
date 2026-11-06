/**
  ******************************************************************************
  * @file    app_proto.c
  * @brief   显示端与采集端通信协议实现
  ******************************************************************************
  */

#include "app_proto.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define PROTO_STREAM_BUF_SIZE   4096
#define PROTO_TEXT_LINE_MAX     128

/* 5 档扫描时基对应的时间分度与采样点间隔 (单位: ns)
 * TB 0: 10ms/div   (dt = 200us = 200,000 ns)
 * TB 1: 5ms/div    (dt = 100us = 100,000 ns)
 * TB 2: 1ms/div    (dt =  20us =  20,000 ns)
 * TB 3: 500us/div  (dt =  10us =  10,000 ns)
 * TB 4: 100us/div  (dt =   2us =   2,000 ns)
 */
static const uint32_t s_tb_dt_ns[SCOPE_TIMEBASE_COUNT] = SCOPE_SAMPLE_NS;

/* 流式滑动窗口缓冲区 */
static uint8_t  s_stream_buf[PROTO_STREAM_BUF_SIZE];
static uint16_t s_stream_len = 0;

/* 最近提取的一帧波形数据 (1513 字节) */
static uint8_t  s_wave_buf[PROTO_BIN_FRAME_LEN];
static uint8_t  s_wave_ready = 0;

/* 最近提取的一行文本 */
static char     s_text_line[PROTO_TEXT_LINE_MAX];

/* 采集端同步状态缓存 */
static proto_state_t s_remote_state;
static proto_stats_t s_stats;
static uint32_t s_last_sequence;
static uint8_t s_have_sequence;

void proto_init(void)
{
    proto_reset();
    memset(&s_stats, 0, sizeof(s_stats));
    s_have_sequence = 0;
    memset(&s_remote_state, 0, sizeof(s_remote_state));
}

void proto_reset(void)
{
    s_stream_len = 0;
    s_wave_ready = 0;
    s_text_line[0] = '\0';
}

static void stream_drop(uint16_t n)
{
    if (n >= s_stream_len) {
        s_stream_len = 0;
        return;
    }
    memmove(s_stream_buf, s_stream_buf + n, s_stream_len - n);
    s_stream_len -= n;
}

void proto_feed(const uint8_t *data, uint16_t len)
{
    if (data == NULL || len == 0) {
        return;
    }

    if (s_stream_len + len > PROTO_STREAM_BUF_SIZE) {
        /* 防溢出截断：清空一半历史数据 */
        stream_drop(s_stream_len / 2);
    }

    if (s_stream_len + len <= PROTO_STREAM_BUF_SIZE) {
        memcpy(s_stream_buf + s_stream_len, data, len);
        s_stream_len += len;
    }
}

/**
  * @brief  解析采集端 $STATE 状态上报行
  *         格式示例: $STATE,TB:2,MODE:1,GEN:1000,RUN:1,TRIG:CH2,RISING,AUTO,LVL:128,TRGD:1\r\n
  */
/* 只有完整且合法的 STATE 才提交，避免半条回复混入旧配置。 */
static void parse_state_line(const char *line)
{
    unsigned int tb, mode, frequency, run, level, triggered;
    char source[24], edge[8], trigger_mode[8], extra;
    static const char *const sources[] = {"CH1", "CH2", "DIG2", "DIG0", "DIG1"};
    static const char *const modes[] = {"AUTO", "NORMAL", "SINGLE"};
    int src, trig_mode;
    s_remote_state.valid = 0;
    if (sscanf(line, "$STATE,TB:%u,MODE:%u,GEN:%u,RUN:%u,TRIG:%23[^,],%7[^,],%7[^,],LVL:%u,TRGD:%u %c",
               &tb, &mode, &frequency, &run, source, edge, trigger_mode, &level, &triggered, &extra) != 9)
        return;
    if (tb >= SCOPE_TIMEBASE_COUNT || mode > 1 || frequency < 100 || frequency > 10000 ||
        run > 1 || level > 255 || triggered > 1) return;
    for (src = 0; src < SCOPE_TRIGGER_SOURCES; src++) {
        size_t n = strlen(sources[src]);
        if (strncmp(source, sources[src], n) == 0 && (source[n] == 0 || source[n] == '(')) break;
    }
    for (trig_mode = 0; trig_mode < 3; trig_mode++)
        if (strcmp(trigger_mode, modes[trig_mode]) == 0) break;
    if (src == SCOPE_TRIGGER_SOURCES || trig_mode == 3 ||
        (strcmp(edge, "RISING") != 0 && strcmp(edge, "FALLING") != 0)) return;
    s_remote_state.timebase = (uint8_t)tb;
    s_remote_state.mode = (uint8_t)mode;
    s_remote_state.gen_freq = frequency;
    s_remote_state.run = (uint8_t)run;
    s_remote_state.trig_src = (uint8_t)src;
    s_remote_state.trig_edge = (uint8_t)(strcmp(edge, "FALLING") == 0);
    s_remote_state.trig_mode = (uint8_t)trig_mode;
    s_remote_state.trig_level = (uint8_t)level;
    s_remote_state.triggered = (uint8_t)triggered;
    s_remote_state.valid = 1;
}

/* 序号自然回绕时仍连续；采集板重启或序号回退不累计为巨量丢帧。 */
static void record_sequence(void)
{
    uint32_t seq = (uint32_t)s_stream_buf[6] | ((uint32_t)s_stream_buf[7] << 8) |
                   ((uint32_t)s_stream_buf[8] << 16) | ((uint32_t)s_stream_buf[9] << 24);
    uint32_t gap = seq - s_last_sequence;
    if (s_have_sequence && gap > 1 && gap < 0x80000000u) {
        uint32_t lost = s_stats.lost_frames + gap - 1;
        s_stats.lost_frames = (uint16_t)(lost > 65535u ? 65535u : lost);
    }
    s_last_sequence = seq;
    s_have_sequence = 1;
}

/* 每次取一帧波形或一行文字；坏数据向前丢一个字节，继续找帧头。 */
proto_res_t proto_poll(void)
{
    while (s_stream_len > 0) {
        uint16_t i, line_len;
        uint8_t checksum = 0;

        if (s_stream_buf[0] == PROTO_FRAME_HEAD0) {
            if (s_stream_len < 3) return PROTO_RES_NONE;
            if (s_stream_buf[1] != PROTO_FRAME_HEAD1 ||
                s_stream_buf[2] != PROTO_FRAME_TYPE_WAVE) {
                stream_drop(1);
                continue;
            }
            if (s_stream_len < SCOPE_HEADER_BYTES) return PROTO_RES_NONE;
            if (s_stream_buf[3] >= SCOPE_TIMEBASE_COUNT ||
                s_stream_buf[4] != (PROTO_FRAME_POINTS & 255u) ||
                s_stream_buf[5] != (PROTO_FRAME_POINTS >> 8)) {
                if (s_stats.bad_frames < 65535u) s_stats.bad_frames++;
                stream_drop(1);
                continue;
            }
            if (s_stream_len < PROTO_BIN_FRAME_LEN) return PROTO_RES_NONE;

            for (i = 2; i < PROTO_BIN_FRAME_LEN - 3; i++) checksum ^= s_stream_buf[i];
            if (checksum != s_stream_buf[PROTO_BIN_FRAME_LEN - 3] ||
                s_stream_buf[PROTO_BIN_FRAME_LEN - 2] != '\r' ||
                s_stream_buf[PROTO_BIN_FRAME_LEN - 1] != '\n') {
                if (s_stats.bad_frames < 65535u) s_stats.bad_frames++;
                stream_drop(1);
                continue;
            }
            record_sequence();
            memcpy(s_wave_buf, s_stream_buf, PROTO_BIN_FRAME_LEN);
            s_wave_ready = 1;
            stream_drop(PROTO_BIN_FRAME_LEN);
            return PROTO_RES_WAVE;
        }

        // 不可打印字节和空行直接跳过。
        if (s_stream_buf[0] < 32 || s_stream_buf[0] > 126) {
            stream_drop(1);
            continue;
        }
        // 文本到换行结束；先遇到波形帧头时，丢弃前面的残缺文本。
        for (i = 0; i < s_stream_len; i++) {
            if (s_stream_buf[i] == '\n' || s_stream_buf[i] == PROTO_FRAME_HEAD0) break;
        }
        if (i == s_stream_len) {
            if (s_stream_len > 256) {
                stream_drop(1);
                continue;
            }
            return PROTO_RES_NONE;
        }
        if (s_stream_buf[i] == PROTO_FRAME_HEAD0) {
            stream_drop(i);
            continue;
        }
        line_len = i;
        if (line_len >= PROTO_TEXT_LINE_MAX) line_len = PROTO_TEXT_LINE_MAX - 1;
        memcpy(s_text_line, s_stream_buf, line_len);
        s_text_line[line_len] = 0;
        if (line_len > 0 && s_text_line[line_len - 1] == '\r') s_text_line[line_len - 1] = 0;
        if (strncmp(s_text_line, "$STATE", 6) == 0) parse_state_line(s_text_line);
        stream_drop(i + 1);
        return PROTO_RES_TEXT;
    }
    return PROTO_RES_NONE;
}

uint8_t proto_get_wave(wave_window_t *w)
{
    if (!s_wave_ready) return 0;
    s_wave_ready = 0;
    if (!w) return 0; /* 等待配置确认时，丢弃在途波形但保留序号统计。 */

    uint8_t tb    = s_wave_buf[3];
    uint16_t count = (uint16_t)s_wave_buf[4] | ((uint16_t)s_wave_buf[5] << 8);

    if (tb >= SCOPE_TIMEBASE_COUNT || count != PROTO_FRAME_POINTS || count > NPT) {
        s_wave_ready = 0;
        return 0;
    }

    w->n           = count;
    w->scan_code   = tb;
    w->dt_ns       = s_tb_dt_ns[tb];

    const uint8_t *payload = &s_wave_buf[10];
    for (uint16_t i = 0; i < count; i++) {
        /* 将 8 位采集压缩码 (0~255) 等比放大到 12 位码 (0~4095) */
        uint8_t raw_ch1 = payload[i * 3 + 0];
        uint8_t raw_ch2 = payload[i * 3 + 1];
        uint8_t dig_m   = payload[i * 3 + 2];

        w->ch1[i] = (uint16_t)(((uint32_t)raw_ch1 * 4095u + 127u) / 255u);
        w->ch2[i] = (uint16_t)(((uint32_t)raw_ch2 * 4095u + 127u) / 255u);

        /* dig_mask 低 3 位: bit0=DIG0音频阈值, bit1=DIG1用户按键, bit2=DIG2方波 */
        w->dig[i] = (uint8_t)(dig_m & 0x07u);
    }

    return 1;
}

const char* proto_get_text(void)
{
    return s_text_line;
}

const proto_state_t* proto_get_remote_state(void)
{
    return &s_remote_state;
}


const proto_stats_t *proto_get_stats(void)
{
    return &s_stats;
}
