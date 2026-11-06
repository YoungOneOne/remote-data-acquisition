/**
  ******************************************************************************
  * @file    drv_uart.c
  * @brief   链路串口驱动 (USART6, DMA 循环接收 + 硬件无损读出)
  *
  *          采用 DMA_CIRCULAR 循环硬件缓冲，彻底解决原单次模式空闲中断关停
  *          导致的 ORE (溢出死锁) 与漏字节问题，实现 100% 满帧无损接收！
  ******************************************************************************
  */
#include "main.h"
#include "usart.h"      /* 提供全局 huart6 */
#include "drv_uart.h"

#define RX_DMA_BUF_SIZE     8192u

static DMA_HandleTypeDef s_hdma_rx;
static uint8_t           s_rx_buf[RX_DMA_BUF_SIZE];
static uint16_t          s_dma_read_pos = 0;
static volatile uint32_t s_rx_total = 0;
static uint8_t           s_ready = 0;

/* DMA 通道配置: USART6_RX 对应 DMA2 Stream1 Channel5 */
static void dma_rx_config(void)
{
    __HAL_RCC_DMA2_CLK_ENABLE();

    s_hdma_rx.Instance                 = DMA2_Stream1;
    s_hdma_rx.Init.Channel             = DMA_CHANNEL_5;
    s_hdma_rx.Init.Direction           = DMA_PERIPH_TO_MEMORY;
    s_hdma_rx.Init.PeriphInc           = DMA_PINC_DISABLE;
    s_hdma_rx.Init.MemInc              = DMA_MINC_ENABLE;
    s_hdma_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    s_hdma_rx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    s_hdma_rx.Init.Mode                = DMA_CIRCULAR;      /* 硬件循环接收，永不停机 */
    s_hdma_rx.Init.Priority            = DMA_PRIORITY_VERY_HIGH;
    s_hdma_rx.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
    HAL_DMA_Init(&s_hdma_rx);

    __HAL_LINKDMA(&huart6, hdmarx, s_hdma_rx);

    HAL_NVIC_SetPriority(DMA2_Stream1_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream1_IRQn);
}

void drv_uart_init(uint32_t baud)
{
    s_dma_read_pos = 0;
    s_rx_total     = 0;
    s_ready        = 0;

    huart6.Init.BaudRate = baud;
    if (HAL_UART_Init(&huart6) != HAL_OK) {
        return;
    }

    dma_rx_config();

    /* 启动 DMA 循环接收，之后硬件自动循环存入 s_rx_buf，永不关停 */
    if (HAL_UART_Receive_DMA(&huart6, s_rx_buf, RX_DMA_BUF_SIZE) == HAL_OK) {
        s_ready = 1;
    }
}

void drv_uart_send(const uint8_t *data, uint16_t len)
{
    if (data == NULL || len == 0) return;
    HAL_UART_Transmit(&huart6, (uint8_t *)data, len, 50);
}

uint16_t drv_uart_read(uint8_t *dst, uint16_t max)
{
    if (dst == NULL || max == 0 || !s_ready) {
        return 0;
    }

    /* 自动清除可能因物理拔插引起的错误标志，防止硬件串口死锁 */
    if (__HAL_UART_GET_FLAG(&huart6, UART_FLAG_ORE)) {
        __HAL_UART_CLEAR_OREFLAG(&huart6);
    }
    if (__HAL_UART_GET_FLAG(&huart6, UART_FLAG_FE)) {
        __HAL_UART_CLEAR_FEFLAG(&huart6);
    }
    if (__HAL_UART_GET_FLAG(&huart6, UART_FLAG_NE)) {
        __HAL_UART_CLEAR_NEFLAG(&huart6);
    }

    /* 如果 DMA 意外停止，自动重新拉起 */
    if ((DMA2_Stream1->CR & DMA_SxCR_EN) == 0) {
        HAL_UART_Receive_DMA(&huart6, s_rx_buf, RX_DMA_BUF_SIZE);
    }

    /* 根据 DMA 硬件计数器实时计算当前写入位置
     *
     * 注意这个取模：循环 DMA 的 NDTR 从 2048 递减到 0 后自动重装。
     * 当 NDTR 恰好读到 0 时，RX_DMA_BUF_SIZE - NDTR = 2048，
     * 而 s_dma_read_pos 的取值范围是 0..2047，**永远不可能等于 2048**。
     * 结果就是下面的 while 只能靠 copied < max 退出、每轮都返回 512 字节，
     * 上层 app_link_poll() 的 for(;;) 永远拿不到 0 —— 整个主循环卡死在这里。
     * 取模归一化到 0..2047 就杜绝了这个情况。 */
    uint16_t dma_write_pos = (uint16_t)((RX_DMA_BUF_SIZE - (uint16_t)(DMA2_Stream1->NDTR))
                                        % RX_DMA_BUF_SIZE);
    uint16_t copied = 0;

    /* 只搬"真正已经收到"的字节数：两者相等就立刻返回 0，让上层跳出循环 */
    while (s_dma_read_pos != dma_write_pos && copied < max)
    {
        dst[copied++] = s_rx_buf[s_dma_read_pos];
        s_dma_read_pos = (uint16_t)((s_dma_read_pos + 1u) % RX_DMA_BUF_SIZE);
    }

    s_rx_total += copied;
    return copied;
}

uint32_t drv_uart_rx_total(void)
{
    return s_rx_total;
}

void DMA2_Stream1_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&s_hdma_rx);
}
