/**
  ******************************************************************************
  * @file    app_proto.h
  * @brief   显示端与采集端通信协议
  *
  *          下行链路 (采集端 -> 显示端):
  *            - 1513 字节紧凑二进制波形数据帧 (0xAA 0x55 0x02 [tb] [count16] [seq32] [payload] [xor] \r\n)
  *            - ASCII 文本应答与通知 ($OK, $STATE, $ERR, [KEY] 等)
  *
  *          上行链路 (显示端 -> 采集端):
  *            - ASCII 字符串控制指令 (TB, GEN, RUN, STOP, TRIG_*, SYNC 等)
  ******************************************************************************
  */

#ifndef __APP_PROTO_H
#define __APP_PROTO_H

#include <stdint.h>
#include "app_gfx.h"        /* wave_window_t */

/* 协议格式常量 */
#define PROTO_FRAME_HEAD0           SCOPE_HEAD0
#define PROTO_FRAME_HEAD1           SCOPE_HEAD1
#define PROTO_FRAME_TYPE_WAVE       SCOPE_WAVE_TYPE
#define PROTO_FRAME_POINTS          SCOPE_FRAME_POINTS
#define PROTO_BIN_FRAME_LEN         SCOPE_FRAME_BYTES

/* 状态同步结构体 (对应采集端 $STATE 应答) */
typedef struct {
    uint8_t  timebase;      /* 0~4 */
    uint8_t  mode;          /* 1=Binary, 0=Text */
    uint32_t gen_freq;      /* 100~10000 Hz */
    uint8_t  run;           /* 1=Run, 0=Stop */
    uint8_t  trig_src;      /* 0=CH1, 1=CH2, 2=DIG2, 3=DIG0, 4=DIG1 */
    uint8_t  trig_edge;     /* 0=RISING, 1=FALLING */
    uint8_t  trig_mode;     /* 0=AUTO, 1=NORMAL, 2=SINGLE */
    uint8_t  trig_level;    /* 0~255 */
    uint8_t  triggered;     /* 1=触发成功, 0=自由走带 */
    uint8_t  valid;         /* 1=收到过有效状态 */
} proto_state_t;

/* 解析结果类型 */
typedef enum {
    PROTO_RES_NONE = 0,     /* 暂未解出完整包 */
    PROTO_RES_WAVE,         /* 解出一帧二进制示波器波形 */
    PROTO_RES_TEXT          /* 解出一行 ASCII 文本 */
} proto_res_t;

/* 协议基础控制接口 */
void                 proto_init(void);
void                 proto_reset(void);
void                 proto_feed(const uint8_t *data, uint16_t len);

/* 流轮询解析 */
proto_res_t          proto_poll(void);

/* 读取最近一帧解出的波形数据到 wave_window_t，成功返回 1 */
uint8_t              proto_get_wave(wave_window_t *w);

/* 读取最近解出的一行文本内容 */
const char*          proto_get_text(void);

/* 获取采集端当前同步状态 */
const proto_state_t* proto_get_remote_state(void);

typedef struct {
    uint16_t lost_frames;  /* 按线上序号统计，主动等待触发丢弃的帧不计入 */
    uint16_t bad_frames;   /* 帧头字段、校验或帧尾错误 */
} proto_stats_t;
const proto_stats_t *proto_get_stats(void);

#endif
