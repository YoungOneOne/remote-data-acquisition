#include "main.h"
#include "usart.h"
#include "drv_uart.h"

#define RX_DMA_BUF_SIZE     8192u

static DMA_HandleTypeDef s_hdma_rx;
static uint8_t           s_rx_buf[RX_DMA_BUF_SIZE];
static uint16_t          s_dma_read_pos = 0;
static volatile uint32_t s_rx_total = 0;
static uint8_t           s_ready = 0;

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
    s_hdma_rx.Init.Mode                = DMA_CIRCULAR;
    s_hdma_rx.Init.Priority            = DMA_PRIORITY_VERY_HIGH;
    s_hdma_rx.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
    HAL_DMA_Init(&s_hdma_rx);

    __HAL_LINKDMA(&huart6, hdmarx, s_hdma_rx);

    HAL_NVIC_SetPriority(DMA2_Stream1_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream1_IRQn);
}

static void uart6_pins_init(void)
{
    GPIO_InitTypeDef g;

    __HAL_RCC_GPIOC_CLK_ENABLE();

    g.Pin       = GPIO_PIN_6 | GPIO_PIN_7;
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF8_USART6;
    HAL_GPIO_Init(GPIOC, &g);
}

void drv_uart_init(uint32_t baud)
{
    s_dma_read_pos = 0;
    s_rx_total     = 0;
    s_ready        = 0;

    huart6.Instance          = USART6;
    huart6.Init.BaudRate     = baud;
    huart6.Init.WordLength   = UART_WORDLENGTH_8B;
    huart6.Init.StopBits     = UART_STOPBITS_1;
    huart6.Init.Parity       = UART_PARITY_NONE;
    huart6.Init.Mode         = UART_MODE_TX_RX;
    huart6.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart6.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart6) != HAL_OK) {
        return;
    }

    uart6_pins_init();

    dma_rx_config();

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

    if (__HAL_UART_GET_FLAG(&huart6, UART_FLAG_ORE)) {
        __HAL_UART_CLEAR_OREFLAG(&huart6);
    }
    if (__HAL_UART_GET_FLAG(&huart6, UART_FLAG_FE)) {
        __HAL_UART_CLEAR_FEFLAG(&huart6);
    }
    if (__HAL_UART_GET_FLAG(&huart6, UART_FLAG_NE)) {
        __HAL_UART_CLEAR_NEFLAG(&huart6);
    }

    if ((DMA2_Stream1->CR & DMA_SxCR_EN) == 0) {
        HAL_UART_Receive_DMA(&huart6, s_rx_buf, RX_DMA_BUF_SIZE);
    }

    uint16_t dma_write_pos = (uint16_t)((RX_DMA_BUF_SIZE - (uint16_t)(DMA2_Stream1->NDTR))
                                        % RX_DMA_BUF_SIZE);
    uint16_t copied = 0;

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
