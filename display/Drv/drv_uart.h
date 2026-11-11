#ifndef __DRV_UART_H
#define __DRV_UART_H

#include <stdint.h>
#include "scope_protocol.h"

#define DRV_UART_BAUD_DEFAULT   SCOPE_BAUD

void drv_uart_init(uint32_t baud);

void drv_uart_send(const uint8_t *data, uint16_t len);

uint16_t drv_uart_read(uint8_t *dst, uint16_t max);

uint32_t drv_uart_rx_total(void);

#endif
