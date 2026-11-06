#include "main.h"
#include "drv_debug.h"
#include <stdio.h>

/* printf 重定向到 SWO/ITM，输出在 Keil 的 Debug (printf) Viewer 里看。
 * 需要在 Options for Target 里勾上 Use MicroLIB。
 * Trace 设置：Core Clock 216MHz，Trace Port 选 SWO UART/NRZ。 */

/* SWO 不通就改成 1，改用串口（板载 ST-LINK 虚拟串口，PA9/PB7，115200）
 * 串口方式不用进调试模式，开个串口助手就能看 */
#define DBG_UART	0

#if DBG_UART
static UART_HandleTypeDef hu;

static void uart_init(void)
{
	GPIO_InitTypeDef g = {0};

	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_USART1_CLK_ENABLE();

	g.Pin = GPIO_PIN_9;									/* PA9 = TX */
	g.Mode = GPIO_MODE_AF_PP;
	g.Pull = GPIO_NOPULL;
	g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	g.Alternate = GPIO_AF7_USART1;
	HAL_GPIO_Init(GPIOA, &g);

	g.Pin = GPIO_PIN_7;									/* PB7 = RX */
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
	/* 使能 ITM。LAR 是解锁寄存器，不写密钥的话 TCR/TER 改了也不生效 */
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

		/* 没接调试器就直接丢字符：ITM 端口靠调试器取数，没人取会一直为 0，
		 * 原版 ITM_SendChar 里的 while 会死等，程序脱机跑就卡死 */
		if ((CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) == 0)
			return ch;

		while (ITM->PORT[0].u32 == 0)				/* 接了调试器但没开 printf 窗口 */
			if (++t > 100000)
				return ch;

		ITM->PORT[0].u8 = (uint8_t)ch;
	}
#endif
	return ch;
}
