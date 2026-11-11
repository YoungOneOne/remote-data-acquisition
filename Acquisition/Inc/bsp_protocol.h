// 串口命令和波形发送接口

#ifndef __BSP_PROTOCOL_H
#define __BSP_PROTOCOL_H

#include "main.h"
#include "bsp_acquisition.h"

// 输出协议格式枚举
typedef enum
{
    PROTOCOL_MODE_TEXT = 0, // 格式化可读文本 (适合串口助手查看)
    PROTOCOL_MODE_BINARY =
        1 // 1513字节二进制紧凑帧 (适合 Python 上位机与开发板 B 接收端)
} ProtocolMode_t;

// 开机初始化一次；主循环在发送波形前处理接收命令
void           Protocol_Init(void);
uint8_t        Protocol_IsBusy(void);
ProtocolMode_t Protocol_GetMode(void);

// 主循环命令处理
void Protocol_ProcessRx(void);

// points 必须包含 500 点；任一路串口忙时本次不发送
// 帧格式：AA 55 02 [tb] [count LE16] [seq LE32] [data...] [xor] CRLF
// LE16 和 LE32 表示低字节先发，接收端按相同顺序还原数字
void Protocol_SendBinaryFrame(Timebase_t tb, const AcqPoint_t *points);

// 发送文本调试信息
void Protocol_PrintDebugText(Timebase_t tb, float ch1_v, float ch2_v, float ch2_vpp,
                             uint8_t dig_mask, uint32_t ovr_err);

#endif // __BSP_PROTOCOL_H
