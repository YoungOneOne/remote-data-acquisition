#include "main.h"
#include "drv_debug.h"
#include <stdio.h>

#define DBG_UART	0

#if DBG_UART
static UART_HandleTypeDef hu;

static void uart_init(void)
{
	GPIO_InitTypeDef g = {0};

	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_USART1_CLK_ENABLE();

	g.Pin = GPIO_PIN_9;
	g.Mode = GPIO_MODE_AF_PP;
	g.Pull = GPIO_NOPULL;
	g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	g.Alternate = GPIO_AF7_USART1;
	HAL_GPIO_Init(GPIOA, &g);

	g.Pin = GPIO_PIN_7;
	HAL_GPIO_Init(GPIOB, &g);

	hu.Instance = USART1;
	hu.Init.BaudRate = 115200;
	hu.Init.WordLength = UART_WORDLENGTH_8B;
	hu.Init.StopBits = UART_STOPBITS_1;
	hu.Init.Parity = UART_PARITY_NONE;
	hu.Init.Mode = UART_MODE_TX_RX;
	hu.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	hu.Init.OverSampling = UART_OVERSAMPLING_16;
	HAL_UART_Init(&hu);
}
#endif

void drv_debug_init(void)
{
#if DBG_UART
	uart_init();
#else

	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	ITM->LAR = 0xC5ACCE55;
	ITM->TCR = ITM_TCR_ITMENA_Msk;
	ITM->TER = 0xFFFFFFFF;
#endif
}

int fputc(int ch, FILE *f)
{
	(void)f;

#if DBG_UART
	HAL_UART_Transmit(&hu, (uint8_t *)&ch, 1, 10);
#else
	{
		uint32_t t = 0;

		if ((CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) == 0)
			return ch;

		while (ITM->PORT[0].u32 == 0)
			if (++t > 100000)
				return ch;

		ITM->PORT[0].u8 = (uint8_t)ch;
	}
#endif
	return ch;
}
