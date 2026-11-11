// 采集参数和函数声明

#ifndef __BSP_ACQUISITION_H
#define __BSP_ACQUISITION_H

#include "main.h"
#include "scope_protocol.h"

// 5 档扫描时基枚举 (10格 = 500采样点，每格50点)
typedef enum
{
    TB_10MS  = 0, // 200 us/sample
    TB_5MS   = 1, // 100 us/sample
    TB_1MS   = 2, // 20 us/sample
    TB_500US = 3, // 10 us/sample
    TB_100US = 4, // 2 us/sample
    TB_COUNT
} Timebase_t;

// 触发源枚举
// 触发源编号是串口协议的一部分，D2 的编号在 D0、D1 前面
// 这里的编号顺序与数字采样字节中的 bit0、bit1、bit2 顺序不同
typedef enum
{
    TRIG_SRC_CH1  = SCOPE_TRIG_CH1,  // CH1 电位器 (PF10 引脚)
    TRIG_SRC_CH2  = SCOPE_TRIG_CH2,  // CH2 麦克风音频 (PA0 引脚，默认)
    TRIG_SRC_DIG2 = SCOPE_TRIG_DIG2, // DIG2 数字输入 (PF9)
    TRIG_SRC_DIG0 = SCOPE_TRIG_DIG0, // D0，PI0
    TRIG_SRC_DIG1 = SCOPE_TRIG_DIG1, // D1，PF8
    TRIG_SRC_COUNT
} TrigSource_t;

// 触发边沿极性枚举
typedef enum
{
    TRIG_EDGE_RISING  = 0, // 上升沿触发
    TRIG_EDGE_FALLING = 1  // 下降沿触发
} TrigEdge_t;

// 示波器触发模式枚举
typedef enum
{
    TRIG_MODE_AUTO   = 0, // 自动模式：检测到边沿对齐，无边沿时也自由刷新
    TRIG_MODE_NORMAL = 1, // 常规模式：只有捕获到边沿才刷新，无信号时波形定格
    TRIG_MODE_SINGLE = 2  // 单次模式：捕获到一次有效边沿后立即定格并停止输出新帧
} TrigMode_t;

// 触发参数
typedef struct
{
    TrigSource_t source;    // 触发源 (两路模拟或三路数字)
    TrigEdge_t   edge;      // 触发边沿 (上升沿, 下降沿)
    TrigMode_t   mode;      // 触发模式 (AUTO, NORMAL, SINGLE)
    uint8_t      level;     // 触发电平阈值 (0~255，默认 128 对应 1.65V 中间电压)
    uint8_t      triggered; // 当前帧是否触发成功标志 (1:已触发, 0:未触发)
} TriggerConfig_t;

// 一个采样点占 3 字节，两路模拟量和一字节数字电平
typedef struct
{
    uint8_t ch1_raw8; // CH1 电位器采样值压缩为 8 位 (0 ~ 255)
    uint8_t ch2_raw8; // CH2 麦克风采样值压缩为 8 位 (0 ~ 255)
    uint8_t dig_mask; // 3 路数字引脚掩码 (bit0: DIG0, bit1: DIG1, bit2: DIG2)
} AcqPoint_t;

#define ACQ_FRAME_POINTS ((int)SCOPE_FRAME_POINTS) // 单屏显示的波形点数

// 一帧波形和测量结果，由调用者提供存储空间
typedef struct
{
    AcqPoint_t points[ACQ_FRAME_POINTS];
    Timebase_t timebase;    // 本帧采样时使用的时基编号
    float      ch1_voltage; // 500 点的平均电压
    float      ch2_voltage; // 500 点的平均电压
    float      ch2_vpp;     // 最大值与最小值的电压差
    uint8_t    digital;     // 最后一个采样点的三路数字电平，用于文本显示
} AcqFrame_t;

// 采集接口
// 开机调用一次；SetTimebase、Resume 和 ARM 在初始化完成后使用
void Acquisition_Init(void);
// 在 TIM2 中断入口调用，返回 1 表示本次中断已处理
uint8_t     Acquisition_TIM2_IRQHandler(void);
void        Acquisition_SetTimebase(Timebase_t tb);
Timebase_t  Acquisition_GetTimebase(void);
const char *Acquisition_GetTimebaseName(Timebase_t tb);

void    Acquisition_Pause(void);
void    Acquisition_Resume(void);
uint8_t Acquisition_IsRunning(void);

uint8_t Acquisition_IsFrameReady(void);
// frame 必须指向完整结果；返回 1 表示成功取得一帧
// 返回 0 时不要发送 frame，里面可能仍是上一次的结果
uint8_t  Acquisition_GetFrame(AcqFrame_t *frame);
uint32_t Acquisition_GetOvrErrorCount(void);

// 触发控制接口
// 参数在命令入口检查，SetTrigger 保存后由 Resume 重新采样
void Acquisition_SetTrigger(TrigSource_t src, TrigEdge_t edge, TrigMode_t mode,
                            uint8_t level);
TriggerConfig_t *Acquisition_GetTriggerConfig(void);
void             Acquisition_ArmSingleTrigger(void);

const char *Acquisition_GetTrigSourceName(TrigSource_t src);
const char *Acquisition_GetTrigEdgeName(TrigEdge_t edge);
const char *Acquisition_GetTrigModeName(TrigMode_t mode);

#endif // __BSP_ACQUISITION_H
