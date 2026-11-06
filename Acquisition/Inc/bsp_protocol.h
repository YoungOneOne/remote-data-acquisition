/**
  ******************************************************************************
  * @file    bsp_protocol.h
  * @brief   通信与协议打包模块头文件 (文本格式与二进制示波器帧)
  ******************************************************************************
  */

#ifndef __BSP_PROTOCOL_H
#define __BSP_PROTOCOL_H

#include "main.h"
#include "bsp_acquisition.h"

/* 输出协议格式枚举 */
typedef enum {
    PROTOCOL_MODE_TEXT = 0,   /* 格式化可读文本 (适合串口助手查看) */
    PROTOCOL_MODE_BINARY = 1  /* 1513字节二进制紧凑帧 (适合 Python 上位机与开发板 B 接收端) */
} ProtocolMode_t;

void           Protocol_Init(void);
uint8_t        Protocol_IsBusy(void);
void           Protocol_SetMode(ProtocolMode_t mode);
ProtocolMode_t Protocol_GetMode(void);
void           Protocol_ProcessRx(void);
void           Protocol_UartRx_ISR(void);  /* USART1 中断调用接口 (ST-Link VCP) */
void           Protocol_Uart6Rx_ISR(void); /* USART6 中断调用接口 (双板直联) */

/* 发送标准二进制示波器数据帧 (0xAA 0x55 0x02 [tb] [count16] [seq32] [data...] [xor] \r\n) */
void Protocol_SendBinaryFrame(Timebase_t tb, const AcqPoint_t *points, uint16_t count);

/* 发送文本调试信息 */
void Protocol_PrintDebugText(Timebase_t tb, float ch1_v, float ch2_v, float ch2_vpp, uint8_t dig_mask, uint32_t ovr_err);

/* 统一调度发送函数 (根据当前 ProtocolMode 自动选择输出文本或二进制) */
void Protocol_OutputFrame(Timebase_t tb, const AcqPoint_t *points, uint16_t count,
                          float ch1_v, float ch2_v, float ch2_vpp, uint8_t dig_mask, uint32_t ovr_err);

#endif /* __BSP_PROTOCOL_H */
