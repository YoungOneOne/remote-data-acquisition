/**
  ******************************************************************************
  * @file    drv_uart.h
  * @brief   链路串口驱动（USART6，DMA + 空闲中断接收）
  *
  *          Drv 层 —— 硬件驱动层
  *
  *          这一层只负责"把字节收进来 / 把字节发出去"，不懂任何协议。
  *          协议解析在 App 层的 app_proto.c。
  ******************************************************************************
  */

#ifndef __DRV_UART_H
#define __DRV_UART_H

#include <stdint.h>
#include "../../../Common/scope_protocol.h"

/* USART6 双板链路：460800、8N1，波形1513字节约需32.8ms。 */
#define DRV_UART_BAUD_DEFAULT   SCOPE_BAUD

/* 初始化：重新设置波特率，并启动 DMA 循环接收。
 * 依赖 CubeMX 已经生成好的 huart6（USART6）。 */
void drv_uart_init(uint32_t baud);

/* 发送一段数据（阻塞方式，发完才返回） */
void drv_uart_send(const uint8_t *data, uint16_t len);

/* 取一段收到的数据。返回实际字节数，0 表示当前没有新数据。
 * 一次调用返回的是"两帧之间连续收到的那一段"，可以理解为一次串口突发。 */
uint16_t drv_uart_read(uint8_t *dst, uint16_t max);

/* 累计收到的字节数，用来判断链路是否有数据进来 */
uint32_t drv_uart_rx_total(void);

#endif /* __DRV_UART_H */
