/**
  ******************************************************************************
  * @file    drv_lcd.c
  * @brief   32F746G-DISCO 板级：FMC SDRAM 初始化 + LTDC 显示控制
  *
  *          SDRAM 时序/引脚全部照抄 ST 官方：
  *            Drivers/BSP/STM32746G-Discovery/stm32746g_discovery_sdram.c
  *            Projects/STM32746G-Discovery/Examples/FMC/FMC_SDRAM/Src/stm32f7xx_hal_msp.c
  *            引脚名对照 UM1907 表 11
  ******************************************************************************
  */
#include "main.h"
#include "drv_lcd.h"
#include "ltdc.h"
#include <stdio.h>

/* --------------------------------------------------------------- 编译期检查
 * SDRAM 的驱动实现（stm32f7xx_hal_sdram.c）被 HAL_SDRAM_MODULE_ENABLED 整体包住，
 * 这个宏定义在 Core/Inc/stm32f7xx_hal_conf.h 里。
 * CubeMX 重新生成代码时会把它注释掉（因为 .ioc 里没启用 FMC），
 * 那时这里会报几十个"未知类型"的错。加一句检查，直接把原因讲清楚。 */
#ifndef HAL_SDRAM_MODULE_ENABLED
#error "HAL_SDRAM_MODULE_ENABLED 被关掉了：请到 Core/Inc/stm32f7xx_hal_conf.h 去掉该行的注释"
#endif

static SDRAM_HandleTypeDef      s_hsdram;
static FMC_SDRAM_CommandTypeDef s_cmd;

/* 0 = SDRAM 不可用（初始化没成功或自检没过），1 = 可用。
 * 上层必须先看这个再决定能不能碰 0xC0000000：
 * 未初始化的 SDRAM 上做访问有可能把 AXI 总线挂住。 */
uint8_t g_sdram_ok = 0;

/* 诊断信息：初始化走到哪一步、自检第一个错在哪
 *   g_sdram_stage: 0=未开始 1=HAL_SDRAM_Init 2=CLK_ENABLE 3=PALL
 *                  4=AUTOREFRESH 5=LOAD_MODE 6=初始化完成 7=写读校验失败 8=全部通过 */
uint8_t  g_sdram_stage     = 0;
uint32_t g_sdram_test_idx  = 0;      /* 第一个读错的下标 */
uint32_t g_sdram_test_want = 0;      /* 期望值 */
uint32_t g_sdram_test_got  = 0;      /* 实际读出 */

/* MT48LC4M32B2 模式寄存器值 = 0x0220
 *   BL=1, 顺序突发, CAS=2, 标准模式, 单次写 */
#define SDRAM_MRD          (0x0000u | 0x0000u | 0x0020u | 0x0000u | 0x0200u)

/* 刷新计数：照抄 D:\qianrushi\F746_Display（那个工程 SDRAM 实测可用）
 *   COUNT = 0x0603 = 1539
 *   ST 的公式 (64ms/4096)*SDCLK-20 在 SDCLK=108MHz 下算得 1667，
 *   但 F746_Display 用 1539 实际工作正常（刷新更频繁，只会更稳，不会出错）。 */
#define SDRAM_REFRESH_CNT  0x0603u
#define SDRAM_CMD_TIMEOUT  0xFFFFu

/* ==========================================================================
 *  FMC 引脚 + 时钟（HAL 的 __weak 回调，这里覆盖掉）
 *  引脚全部来自 UM1907 表 11
 * ========================================================================== */
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
	/* 这两行必须和 ST 官方 / F746_Display 一模一样：
	 *   NOPULL  —— 上拉会给 108MHz 的数据线加 RC，上升沿变慢，读回来就错位
	 *   VERY_HIGH —— HIGH 档只有 ~50MHz，跑不动 108MHz 的 SDCLK */
	g.Pull      = GPIO_NOPULL;
	g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
	g.Alternate = GPIO_AF12_FMC;

	g.Pin = GPIO_PIN_3;                             /* PC3  = FMC_SDCKE0 */
	HAL_GPIO_Init(GPIOC, &g);

	g.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_8 | GPIO_PIN_9 |
	        GPIO_PIN_10 | GPIO_PIN_14 | GPIO_PIN_15; /* PD0/1 = D2/D3, D8..D10 = D13..D15, D14/15 = D0/D1 */
	HAL_GPIO_Init(GPIOD, &g);

	g.Pin = GPIO_PIN_0 | GPIO_PIN_1 |                /* PE0/1 = NBL0/NBL1 */
	        GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 |
	        GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 |
	        GPIO_PIN_14 | GPIO_PIN_15;               /* PE7..PE15 = D4..D12 */
	HAL_GPIO_Init(GPIOE, &g);

	g.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 |
	        GPIO_PIN_4 | GPIO_PIN_5 |                /* PF0..PF5 = A0..A5 */
	        GPIO_PIN_11 |                            /* PF11 = SDNRAS */
	        GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 |
	        GPIO_PIN_15;                             /* PF12..PF15 = A6..A9 */
	HAL_GPIO_Init(GPIOF, &g);

	g.Pin = GPIO_PIN_0 | GPIO_PIN_1 |                /* PG0/1  = A10/A11 */
	        GPIO_PIN_4 | GPIO_PIN_5 |                /* PG4/5  = BA0/BA1 */
	        GPIO_PIN_8 |                             /* PG8    = SDCLK */
	        GPIO_PIN_15;                             /* PG15   = SDNCAS */
	HAL_GPIO_Init(GPIOG, &g);

	g.Pin = GPIO_PIN_3 | GPIO_PIN_5;                 /* PH3 = SDNE0, PH5 = SDNWE */
	HAL_GPIO_Init(GPIOH, &g);
}

/* ==========================================================================
 *  SDRAM 初始化
 * ========================================================================== */
uint8_t drv_sdram_init(void)
{
	FMC_SDRAM_TimingTypeDef timing = {0};
	/* Instance 的类型是 FMC_SDRAM_TypeDef *（指针），直接赋 FMC_SDRAM_DEVICE。
	 *    绝不能写 FMC_SDRAM_BANK1 —— 它的值是 0x00000000，而 0 是合法的空指针常量，
	 *    编译期【不会报错】，结果 Instance = NULL，HAL_SDRAM_Init 内部
	 *        FMC_SDRAM_Init(hsdram->Instance, &hsdram->Init);
	 *    就把 FMC 寄存器全写到 0x0000xxxx（ITCM）去了，SDRAM 控制器根本没配，
	 *    0xC0000000 读回来全是 0xFFFF —— 白屏。 */
	s_hsdram.Instance = FMC_SDRAM_DEVICE;		/* = FMC_Bank5_6 = 0xA0000140 */

	/* 全部参数逐字照抄 ST 官方 F746G-DISCO 例程：
	 *    STM32Cube_FW_F7_V1.17.4\Projects\STM32746G-Discovery\
	 *        Examples\FMC\FMC_SDRAM\Src\main.c  第 97~112 行
	 *    （这是 ST 保证能跑的参考配置，不要自己算）
	 *
	 *    ExitSelfRefreshDelay / RowCycleDelay 官方是 6，之前写 7 是错的 */
	timing.LoadToActiveDelay    = 2;
	timing.ExitSelfRefreshDelay = 6;      /* 官方 = 6 */
  timing.SelfRefreshTime      = 4;
	timing.RowCycleDelay        = 6;      /* 官方 = 6 */
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
	s_hsdram.Init.SDClockPeriod      = FMC_SDRAM_CLOCK_PERIOD_2;   /* 官方 main.h:35 */
	s_hsdram.Init.ReadBurst          = FMC_SDRAM_RBURST_ENABLE;
	s_hsdram.Init.ReadPipeDelay      = FMC_SDRAM_RPIPE_DELAY_0;

	if (HAL_SDRAM_Init(&s_hsdram, &timing) != HAL_OK)
	{
		g_sdram_stage = 1;
		return BSP_ERROR;
	}

	/* ---- 标准 SDRAM 上电初始化序列 ---- */
	s_cmd.CommandTarget          = FMC_SDRAM_CMD_TARGET_BANK1;
	s_cmd.AutoRefreshNumber      = 1;
	s_cmd.ModeRegisterDefinition = 0;

	/* 1) 时钟使能命令 */
	s_cmd.CommandMode = FMC_SDRAM_CMD_CLK_ENABLE;
	if (HAL_SDRAM_SendCommand(&s_hsdram, &s_cmd, SDRAM_CMD_TIMEOUT) != HAL_OK)
	{
		g_sdram_stage = 2;
		return BSP_ERROR;
	}

	/* 2) 至少等 100us */
	HAL_Delay(1);

	/* 3) 预充电所有 bank */
	s_cmd.CommandMode = FMC_SDRAM_CMD_PALL;
	if (HAL_SDRAM_SendCommand(&s_hsdram, &s_cmd, SDRAM_CMD_TIMEOUT) != HAL_OK)
	{
		g_sdram_stage = 3;
		return BSP_ERROR;
	}

	/* 4) 自动刷新 x8 */
	s_cmd.CommandMode       = FMC_SDRAM_CMD_AUTOREFRESH_MODE;
	s_cmd.AutoRefreshNumber = 8;
	if (HAL_SDRAM_SendCommand(&s_hsdram, &s_cmd, SDRAM_CMD_TIMEOUT) != HAL_OK)
	{
		g_sdram_stage = 4;
		return BSP_ERROR;
	}

	/* 5) 写模式寄存器 */
	s_cmd.CommandMode            = FMC_SDRAM_CMD_LOAD_MODE;
	s_cmd.AutoRefreshNumber      = 1;
	s_cmd.ModeRegisterDefinition = SDRAM_MRD;
	if (HAL_SDRAM_SendCommand(&s_hsdram, &s_cmd, SDRAM_CMD_TIMEOUT) != HAL_OK)
	{
		g_sdram_stage = 5;
		return BSP_ERROR;
	}

	/* 6) 刷新计数器 */
	HAL_SDRAM_ProgramRefreshRate(&s_hsdram, SDRAM_REFRESH_CNT);

	g_sdram_stage = 6;                   /* 走到 6 = 初始化步骤全过 */
	g_sdram_ok = 1;
	return BSP_OK;
}

/* 自检：扫满三块帧缓冲占用的整段 3MB（0xC00000 ~ 0xC02FFFFF）。
 *
 *   只测开头 64 KB 是不够的 —— 那样只用到低 15 根地址线，
 *   而 FB1/FB2 在 1MB/2MB 偏移处，要靠行地址 A10/A11 和 BANK 地址 BA0/BA1。
 *   那几根线没接好时，开头照样全过，但一写到 1MB 之后就是总线错误，
 *   屏幕必然全黑（而且 M7 的写错误是非精确的，会在几条指令之后才触发 Fault，
 *   所以"日志在一行中间突然断掉"这种症状也对得上）。
 *
 *   测完会往帧缓冲里写脏数据，ui_init() 随后会把三块全刷成背景色，不影响显示。 */
uint8_t drv_sdram_test(void)
{
	volatile uint32_t *p;
	uint32_t k, i, v;

	if (g_sdram_ok == 0)                 /* 初始化都没成功，别去碰 SDRAM */
		return BSP_ERROR;

	/* 分 3 段（每段 1MB）走，每段前打一行日志。
	 *   万一某段触发总线错误，日志就停在那一行 —— 一眼看出坏在哪一段。 */
	for (k = 0; k < 3u; k++)
	{
		p = (volatile uint32_t *)(SDRAM_BASE + k * 0x00100000u);

		printf("      sdram sweep +0x%06lX ...\r\n",
		       (unsigned long)(k * 0x00100000u));

		for (i = 0; i < 0x40000u; i++)       /* 0x40000 x 4B = 1 MB */
			p[i] = 0xA5A50000u | (i & 0xFFFFu);

		for (i = 0; i < 0x40000u; i++)
		{
			v = p[i];
			if (v != (0xA5A50000u | (i & 0xFFFFu)))
			{
				/* 记下第一个出错的位置和值，方便判断是"全错"还是"偶发" */
				g_sdram_test_idx  = k * 0x00100000u + i * 4u;
				g_sdram_test_want = 0xA5A50000u | (i & 0xFFFFu);
				g_sdram_test_got  = v;
				g_sdram_stage     = 7;
				g_sdram_ok = 0;              /* 读写不对，标记不可用 */
				return BSP_ERROR;
			}
		}
	}

	g_sdram_stage = 8;                   /* 走到 8 = 读写校验也通过 */
	return BSP_OK;
}

/* 打印 FMC 控制寄存器的实际值（诊断用）
 *
 *   如果 SDCR1 / SDTR1 读回来是 0，说明我们对 0xA0000000 的写根本没落到寄存器上
 *   —— 那就是 MPU 没把 FMC 那 8KB 开放出来（region2 配错了）。
 *   如果值正常，说明控制器配好了，问题在 SDRAM 芯片侧（时序/引脚/焊接）。 */
void drv_sdram_dump(void)
{
	printf("  FMC SDCR1=%08lX SDTR1=%08lX SDRTR=%08lX SDSR=%08lX\r\n",
	       (unsigned long)FMC_Bank5_6->SDCR[0],
	       (unsigned long)FMC_Bank5_6->SDTR[0],
	       (unsigned long)FMC_Bank5_6->SDRTR,
	       (unsigned long)FMC_Bank5_6->SDSR);
}

/* ==========================================================================
 *  LTDC
 * ========================================================================== */

uint8_t drv_lcd_init(uint16_t *fb_addr)
{
	if (fb_addr == 0)
		return BSP_ERROR;

	HAL_LTDC_SetAddress(&hltdc, (uint32_t)fb_addr, 0);
	return BSP_OK;
}

/* 提交翻页但不等待硬件完成。调用方要用三块缓冲轮转，
 * 保证接下来画的不是刚提交的这块。 */
void drv_lcd_set_fb_nowait(uint16_t *fb_addr)
{
	if (fb_addr == 0)
		return;

	(void)HAL_LTDC_SetAddress_NoReload(&hltdc, (uint32_t)fb_addr, 0);

	/* 清掉上次残留的重载完成标志，再提交 VBR */
	(void)HAL_LTDC_Reload(&hltdc, LTDC_RELOAD_VERTICAL_BLANKING);
}

/* 等一个垂直消隐边界：先等进入同步期，再等它结束。
 * 返回时屏幕刚开始扫描新的一帧，可以用它做刷新节拍。 */
void drv_lcd_wait_vsync(void)
{
	uint32_t guard;

	/* 1) 等进入垂直同步期 */
	guard = 4000000u;
	while ((hltdc.Instance->CDSR & LTDC_CDSR_VSYNCS) == 0u && guard--)
		;

	/* 2) 等走出垂直同步期（返回时正好在新帧开头） */
	guard = 4000000u;
	while ((hltdc.Instance->CDSR & LTDC_CDSR_VSYNCS) != 0u && guard--)
		;
}

/* 保留旧名字做兼容（原来用于等寄存器重载完成） */
void drv_lcd_wait_reload(void)
{
	drv_lcd_wait_vsync();
}

/* 提交翻页并【等待】完成 —— 需要确定切换已完成时用
 *  （比如初始化阶段要把 LTDC 切到第一块缓冲再开始画） */
void drv_lcd_set_fb(uint16_t *fb_addr)
{
	if (fb_addr == 0)
		return;

	(void)HAL_LTDC_SetAddress_NoReload(&hltdc, (uint32_t)fb_addr, 0);

	hltdc.Instance->ICR = LTDC_ICR_CRRIF;         /* 清旧标志 */
	hltdc.Instance->SRCR = LTDC_SRCR_VBR;         /* 提交 VBR */

	/* 等重载完成（进入垂直消隐后被硬件执行）。正常最多 16.7ms。 */
	{
		uint32_t guard = 2000000u;                /* 防呆 */

		while ((hltdc.Instance->ISR & LTDC_ISR_RRIF) == 0u && guard--)
			;

		hltdc.Instance->ICR = LTDC_ICR_CRRIF;     /* 清标志 */
	}
}

void drv_lcd_backlight(uint8_t on)
{
	/* 32F746G-DISCO：PK3 = LCD_BL_CTRL（背光），PI12 = LCD_DISP（面板使能）
	 * 两脚由 MX_GPIO_Init() 配成推挽输出并已拉高，这里只切背光。
	 * 背光不拉高就是全黑，LTDC 跑得再对也看不见。 */
	HAL_GPIO_WritePin(GPIOK, GPIO_PIN_3, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOI, GPIO_PIN_12, GPIO_PIN_SET);        /* 面板使能常开 */
}
