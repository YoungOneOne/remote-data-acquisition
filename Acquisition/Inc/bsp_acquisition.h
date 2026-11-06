/**
  ******************************************************************************
  * @file    bsp_acquisition.h
  * @brief   ADC3 双通道模拟量与 3 路数字通道采集驱动头文件
  ******************************************************************************
  */

#ifndef __BSP_ACQUISITION_H
#define __BSP_ACQUISITION_H

#include "main.h"
#include "../../Common/scope_protocol.h"

// 5 档扫描时基枚举 (10格 = 500采样点，每格50点)
typedef enum {
    TB_10MS = 0, /* 200 us/sample */
    TB_5MS = 1,  /* 100 us/sample */
    TB_1MS = 2,  /* 20 us/sample */
    TB_500US = 3,/* 10 us/sample */
    TB_100US = 4,/* 2 us/sample */
    TB_COUNT
} Timebase_t;

// 触发源枚举
typedef enum {
    TRIG_SRC_CH1  = SCOPE_TRIG_CH1,  // CH1 电位器 (PF10 引脚)
    TRIG_SRC_CH2  = SCOPE_TRIG_CH2,  // CH2 麦克风音频 (PA0 引脚，默认)
    TRIG_SRC_DIG2 = SCOPE_TRIG_DIG2,  // DIG2 测试方波回环 (PI2 引脚)
    TRIG_SRC_DIG0 = SCOPE_TRIG_DIG0,
    TRIG_SRC_DIG1 = SCOPE_TRIG_DIG1,
    TRIG_SRC_COUNT
} TrigSource_t;

// 触发边沿极性枚举
typedef enum {
    TRIG_EDGE_RISING  = 0,  // 上升沿触发
    TRIG_EDGE_FALLING = 1   // 下降沿触发
} TrigEdge_t;

// 示波器触发模式枚举
typedef enum {
    TRIG_MODE_AUTO   = 0,  // 自动模式：检测到边沿对齐，无边沿时也自由刷新
    TRIG_MODE_NORMAL = 1,  // 常规模式：只有捕获到边沿才刷新，无信号时波形定格
    TRIG_MODE_SINGLE = 2   // 单次模式：捕获到一次有效边沿后立即定格并停止输出新帧
} TrigMode_t;

// 触发配置结构体
typedef struct {
    TrigSource_t source;    // 触发源 (两路模拟或三路数字)
    TrigEdge_t   edge;      // 触发边沿 (上升沿, 下降沿)
    TrigMode_t   mode;      // 触发模式 (AUTO, NORMAL, SINGLE)
    uint8_t      level;     // 触发电平阈值 (0~255，默认 128 对应 1.65V 中间电压)
    uint8_t      triggered; // 当前帧是否触发成功标志 (1:已触发, 0:未触发)
} TriggerConfig_t;

// 单个复合采样点结构体 (3 字节紧凑型)
typedef struct {
    uint8_t ch1_raw8;   // CH1 电位器采样值压缩为 8 位 (0 ~ 255)
    uint8_t ch2_raw8;   // CH2 麦克风采样值压缩为 8 位 (0 ~ 255)
    uint8_t dig_mask;   // 3 路数字引脚掩码 (bit0: DIG0, bit1: DIG1, bit2: DIG2)
} AcqPoint_t;

#define ACQ_FRAME_POINTS    ((int)SCOPE_FRAME_POINTS)    // 单屏显示的波形点数

// 外部调用的函数接口
void              Acquisition_Init(void);
void              Acquisition_SetTimebase(Timebase_t tb);
Timebase_t        Acquisition_GetTimebase(void);
const char*       Acquisition_GetTimebaseName(Timebase_t tb);

void              Acquisition_Pause(void);
void              Acquisition_Resume(void);
uint8_t           Acquisition_IsRunning(void);

uint8_t           Acquisition_IsFrameReady(void);
uint8_t           Acquisition_GetFrame(AcqPoint_t *out_points, float *out_ch1_v, float *out_ch2_v, float *out_ch2_vpp, uint8_t *out_dig_mask);
uint32_t          Acquisition_GetOvrErrorCount(void);
Timebase_t        Acquisition_GetFrameTimebase(void);

// 触发控制接口
void              Acquisition_SetTrigger(TrigSource_t src, TrigEdge_t edge, TrigMode_t mode, uint8_t level);
TriggerConfig_t*  Acquisition_GetTriggerConfig(void);
void              Acquisition_ArmSingleTrigger(void);

const char*       Acquisition_GetTrigSourceName(TrigSource_t src);
const char*       Acquisition_GetTrigEdgeName(TrigEdge_t edge);
const char*       Acquisition_GetTrigModeName(TrigMode_t mode);

#endif /* __BSP_ACQUISITION_H */
