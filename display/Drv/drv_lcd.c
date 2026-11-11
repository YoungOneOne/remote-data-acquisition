#include "main.h"
#include "drv_lcd.h"
#include "ltdc.h"
#include <stdio.h>

#ifndef HAL_SDRAM_MODULE_ENABLED
#error "HAL_SDRAM_MODULE_ENABLED 被关掉了：请到 Core/Inc/stm32f7xx_hal_conf.h 去掉该行的注释"
#endif

static SDRAM_HandleTypeDef      s_hsdram;
static FMC_SDRAM_CommandTypeDef s_cmd;

uint8_t g_sdram_ok = 0;

uint8_t  g_sdram_stage     = 0;
uint32_t g_sdram_test_idx  = 0;
uint32_t g_sdram_test_want = 0;
uint32_t g_sdram_test_got  = 0;

#define SDRAM_MRD          (0x0000u | 0x0000u | 0x0020u | 0x0000u | 0x0200u)

#define SDRAM_REFRESH_CNT  0x0603u
#define SDRAM_CMD_TIMEOUT  0xFFFFu

void HAL_SDRAM_MspInit(SDRAM_HandleTypeDef *hsdram)
{
	GPIO_InitTypeDef g = {0};

	(void)hsdram;

	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOD_CLK_ENABLE();
	__HAL_RCC_GPIOE_CLK_ENABLE();
	__HAL_RCC_GPIOF_CLK_ENABLE();
	__HAL_RCC_GPIOG_CLK_ENABLE();
	__HAL_RCC_GPIOH_CLK_ENABLE();
	__HAL_RCC_FMC_CLK_ENABLE();

	g.Mode      = GPIO_MODE_AF_PP;

	g.Pull      = GPIO_NOPULL;
	g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
	g.Alternate = GPIO_AF12_FMC;

	g.Pin = GPIO_PIN_3;
	HAL_GPIO_Init(GPIOC, &g);

	g.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_8 | GPIO_PIN_9 |
	        GPIO_PIN_10 | GPIO_PIN_14 | GPIO_PIN_15;
	HAL_GPIO_Init(GPIOD, &g);

	g.Pin = GPIO_PIN_0 | GPIO_PIN_1 |
	        GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 |
	        GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 |
	        GPIO_PIN_14 | GPIO_PIN_15;
	HAL_GPIO_Init(GPIOE, &g);

	g.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 |
	        GPIO_PIN_4 | GPIO_PIN_5 |
	        GPIO_PIN_11 |
	        GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 |
	        GPIO_PIN_15;
	HAL_GPIO_Init(GPIOF, &g);

	g.Pin = GPIO_PIN_0 | GPIO_PIN_1 |
	        GPIO_PIN_4 | GPIO_PIN_5 |
	        GPIO_PIN_8 |
	        GPIO_PIN_15;
	HAL_GPIO_Init(GPIOG, &g);

	g.Pin = GPIO_PIN_3 | GPIO_PIN_5;
	HAL_GPIO_Init(GPIOH, &g);
}

uint8_t drv_sdram_init(void)
{
	FMC_SDRAM_TimingTypeDef timing = {0};

	s_hsdram.Instance = FMC_SDRAM_DEVICE;

	timing.LoadToActiveDelay    = 2;
	timing.ExitSelfRefreshDelay = 6;
  timing.SelfRefreshTime      = 4;
	timing.RowCycleDelay        = 6;
	timing.WriteRecoveryTime    = 2;
	timing.RPDelay              = 2;
	timing.RCDDelay             = 2;

	s_hsdram.Init.SDBank             = FMC_SDRAM_BANK1;
	s_hsdram.Init.ColumnBitsNumber   = FMC_SDRAM_COLUMN_BITS_NUM_8;
	s_hsdram.Init.RowBitsNumber      = FMC_SDRAM_ROW_BITS_NUM_12;
	s_hsdram.Init.MemoryDataWidth    = FMC_SDRAM_MEM_BUS_WIDTH_16;
	s_hsdram.Init.InternalBankNumber = FMC_SDRAM_INTERN_BANKS_NUM_4;
	s_hsdram.Init.CASLatency         = FMC_SDRAM_CAS_LATENCY_2;
	s_hsdram.Init.WriteProtection    = FMC_SDRAM_WRITE_PROTECTION_DISABLE;
	s_hsdram.Init.SDClockPeriod      = FMC_SDRAM_CLOCK_PERIOD_2;
	s_hsdram.Init.ReadBurst          = FMC_SDRAM_RBURST_ENABLE;
	s_hsdram.Init.ReadPipeDelay      = FMC_SDRAM_RPIPE_DELAY_0;

	if (HAL_SDRAM_Init(&s_hsdram, &timing) != HAL_OK)
	{
		g_sdram_stage = 1;
		return BSP_ERROR;
	}

	s_cmd.CommandTarget          = FMC_SDRAM_CMD_TARGET_BANK1;
	s_cmd.AutoRefreshNumber      = 1;
	s_cmd.ModeRegisterDefinition = 0;

	s_cmd.CommandMode = FMC_SDRAM_CMD_CLK_ENABLE;
	if (HAL_SDRAM_SendCommand(&s_hsdram, &s_cmd, SDRAM_CMD_TIMEOUT) != HAL_OK)
	{
		g_sdram_stage = 2;
		return BSP_ERROR;
	}

	HAL_Delay(1);

	s_cmd.CommandMode = FMC_SDRAM_CMD_PALL;
	if (HAL_SDRAM_SendCommand(&s_hsdram, &s_cmd, SDRAM_CMD_TIMEOUT) != HAL_OK)
	{
		g_sdram_stage = 3;
		return BSP_ERROR;
	}

	s_cmd.CommandMode       = FMC_SDRAM_CMD_AUTOREFRESH_MODE;
	s_cmd.AutoRefreshNumber = 8;
	if (HAL_SDRAM_SendCommand(&s_hsdram, &s_cmd, SDRAM_CMD_TIMEOUT) != HAL_OK)
	{
		g_sdram_stage = 4;
		return BSP_ERROR;
	}

	s_cmd.CommandMode            = FMC_SDRAM_CMD_LOAD_MODE;
	s_cmd.AutoRefreshNumber      = 1;
	s_cmd.ModeRegisterDefinition = SDRAM_MRD;
	if (HAL_SDRAM_SendCommand(&s_hsdram, &s_cmd, SDRAM_CMD_TIMEOUT) != HAL_OK)
	{
		g_sdram_stage = 5;
		return BSP_ERROR;
	}

	HAL_SDRAM_ProgramRefreshRate(&s_hsdram, SDRAM_REFRESH_CNT);

	g_sdram_stage = 6;
	g_sdram_ok = 1;
	return BSP_OK;
}

uint8_t drv_sdram_test(void)
{
	volatile uint32_t *p;
	uint32_t k, i, v;

	if (g_sdram_ok == 0)
		return BSP_ERROR;

	for (k = 0; k < 3u; k++)
	{
		p = (volatile uint32_t *)(SDRAM_BASE + k * 0x00100000u);

		printf("      sdram sweep +0x%06lX ...\r\n",
		       (unsigned long)(k * 0x00100000u));

		for (i = 0; i < 0x40000u; i++)
			p[i] = 0xA5A50000u | (i & 0xFFFFu);

		for (i = 0; i < 0x40000u; i++)
		{
			v = p[i];
			if (v != (0xA5A50000u | (i & 0xFFFFu)))
			{

				g_sdram_test_idx  = k * 0x00100000u + i * 4u;
				g_sdram_test_want = 0xA5A50000u | (i & 0xFFFFu);
				g_sdram_test_got  = v;
				g_sdram_stage     = 7;
				g_sdram_ok = 0;
				return BSP_ERROR;
			}
		}
	}

	g_sdram_stage = 8;
	return BSP_OK;
}

void drv_sdram_dump(void)
{
	printf("  FMC SDCR1=%08lX SDTR1=%08lX SDRTR=%08lX SDSR=%08lX\r\n",
	       (unsigned long)FMC_Bank5_6->SDCR[0],
	       (unsigned long)FMC_Bank5_6->SDTR[0],
	       (unsigned long)FMC_Bank5_6->SDRTR,
	       (unsigned long)FMC_Bank5_6->SDSR);
}

uint8_t drv_lcd_init(uint16_t *fb_addr)
{
	if (fb_addr == 0)
		return BSP_ERROR;

	HAL_LTDC_SetAddress(&hltdc, (uint32_t)fb_addr, 0);
	return BSP_OK;
}

void drv_lcd_set_fb_nowait(uint16_t *fb_addr)
{
	if (fb_addr == 0)
		return;

	(void)HAL_LTDC_SetAddress_NoReload(&hltdc, (uint32_t)fb_addr, 0);

	(void)HAL_LTDC_Reload(&hltdc, LTDC_RELOAD_VERTICAL_BLANKING);
}

void drv_lcd_wait_vsync(void)
{
	uint32_t guard;

	guard = 4000000u;
	while ((hltdc.Instance->CDSR & LTDC_CDSR_VSYNCS) == 0u && guard--)
		;

	guard = 4000000u;
	while ((hltdc.Instance->CDSR & LTDC_CDSR_VSYNCS) != 0u && guard--)
		;
}

void drv_lcd_wait_reload(void)
{
	drv_lcd_wait_vsync();
}

void drv_lcd_set_fb(uint16_t *fb_addr)
{
	if (fb_addr == 0)
		return;

	(void)HAL_LTDC_SetAddress_NoReload(&hltdc, (uint32_t)fb_addr, 0);

	hltdc.Instance->ICR = LTDC_ICR_CRRIF;
	hltdc.Instance->SRCR = LTDC_SRCR_VBR;

	{
		uint32_t guard = 2000000u;

		while ((hltdc.Instance->ISR & LTDC_ISR_RRIF) == 0u && guard--)
			;

		hltdc.Instance->ICR = LTDC_ICR_CRRIF;
	}
}

void drv_lcd_backlight(uint8_t on)
{

	HAL_GPIO_WritePin(GPIOK, GPIO_PIN_3, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOI, GPIO_PIN_12, GPIO_PIN_SET);
}
