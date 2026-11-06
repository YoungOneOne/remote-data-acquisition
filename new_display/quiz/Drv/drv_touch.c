/**
  ******************************************************************************
  * @file    touch.c
  * @brief   FT5336 电容触摸屏驱动（I2C3，轮询方式）
  ******************************************************************************
  */
#include "main.h"
#include "i2c.h"			/* 提供 hi2c3 */
#include "drv_touch.h"

/* FT5336 寄存器 */
#define REG_TD_STATUS       0x02
#define REG_TOUCH1_XH       0x03		/* 一次连读 0x02~0x06 共 5 字节 */
#define REG_GMODE           0xA4
#define REG_CHIP_ID         0xA8

/* I2C 超时时间。一次读 5 字节约 0.2ms，2ms 足够；
 * 设太大会在总线异常时长时间阻塞主循环。 */
#define TOUCH_I2C_TMO       2			/* ms，一次读 5 字节足够 */

static uint8_t s_chip_id;

/* ==========================================================================
 *  初始化
 * ========================================================================== */
uint8_t drv_touch_init(void)
{
	uint8_t mode = 0x00;				/* 0 = 轮询模式 */

	s_chip_id = 0;

	/* 1) 器件在不在 */
	if (HAL_I2C_IsDeviceReady(&hi2c3, TOUCH_I2C_ADDR, 3, 100) != HAL_OK)
		return TOUCH_ERR;

	/* 2) 读芯片 ID（只为了调试时能打印出来，不拿它当判据） */
	(void)HAL_I2C_Mem_Read(&hi2c3, TOUCH_I2C_ADDR, REG_CHIP_ID,
	                       I2C_MEMADD_SIZE_8BIT, &s_chip_id, 1, 100);

	/* 3) 切到轮询模式。有些批次上电默认是触发模式，
	 *    不写这句 TD_STATUS 永远是 0，按屏幕也没反应 */
	(void)HAL_I2C_Mem_Write(&hi2c3, TOUCH_I2C_ADDR, REG_GMODE,
	                        I2C_MEMADD_SIZE_8BIT, &mode, 1, 100);

	return TOUCH_OK;
}

uint8_t drv_touch_chip_id(void)
{
	return s_chip_id;
}

/* ==========================================================================
 *  读一次
 * ========================================================================== */
uint8_t drv_touch_scan(Touch_State *ts)
{
	uint8_t  buf[5];
	uint16_t raw_x, raw_y;

	if (ts == 0)
		return TOUCH_ERR;

	ts->pressed = 0;
	ts->count   = 0;

	/* 一次连读 5 字节：TD_STATUS, XH, XL, YH, YL
	 * 必须连续读，分开读的话 X 和 Y 可能不是同一时刻的，坐标会乱跳 */
	if (HAL_I2C_Mem_Read(&hi2c3, TOUCH_I2C_ADDR, REG_TD_STATUS,
	                     I2C_MEMADD_SIZE_8BIT, buf, 5, TOUCH_I2C_TMO) != HAL_OK)
		return TOUCH_ERR;

	ts->count = (uint8_t)(buf[0] & 0x0Fu);
	if (ts->count == 0)
		return TOUCH_OK;				/* 手指松开了 */

	/* 12 位坐标：高 4 位在前一个寄存器的低半字节
	 * & 0x0F 不能省，高 4 位是事件标志，不屏蔽坐标会变成 16000 多 */
	raw_x = (uint16_t)(((uint16_t)(buf[1] & 0x0Fu) << 8) | buf[2]);
	raw_y = (uint16_t)(((uint16_t)(buf[3] & 0x0Fu) << 8) | buf[4]);

	/* 面板出厂按竖屏（272 宽 x 480 高）装，板子 LCD 是横屏 480x272，
	 * 所以原始 Y 对应屏幕 X、原始 X 对应屏幕 Y */
	ts->x = raw_y;
	ts->y = raw_x;

	if (ts->x >= TOUCH_MAX_X || ts->y >= TOUCH_MAX_Y)
		return TOUCH_OK;				/* 越界数据丢掉 */

	ts->pressed = 1;
	return TOUCH_OK;
}

/* 最近一次 Poll 的状态，供界面层读 */
static Touch_State s_last;

const Touch_State *drv_touch_last(void)
{
	return &s_last;
}

/* ==========================================================================
 *  轮询 + 边沿检测
 * ========================================================================== */

#define TOUCH_FAIL_MAX      3       /* 连续读失败这么多次才认"松开" */

uint8_t drv_touch_poll(Touch_State *ts)
{
	static Touch_State s_prev;
	static uint8_t     s_init;
	static uint8_t     s_fail;

	Touch_State now;
	uint8_t     ev = 0;

	if (ts == 0)
		return 0;

	if (drv_touch_scan(&now) != TOUCH_OK)
	{
		/* I2C 偶发错误不能立刻当成"松开"：
		 *   那样会造出一个假的 抬起+按下，把一次点击算成两次。
		 *   连续失败够多次才真的报松开。 */
		if (++s_fail < TOUCH_FAIL_MAX)
		{
			*ts = s_prev;
			return 0;				/* 沿用上一次状态，不上报事件 */
		}

		s_fail      = 0;
		now.pressed = 0;
		now.count   = 0;
		now.x       = s_prev.x;
		now.y       = s_prev.y;
	}
	else
	{
		s_fail = 0;
	}

	if (!s_init)
	{
		s_prev = now;
		s_init = 1;
	}

	if (now.pressed && !s_prev.pressed)
		ev |= TOUCH_EV_DOWN;
	if (!now.pressed && s_prev.pressed)
		ev |= TOUCH_EV_UP;
	if (now.pressed && s_prev.pressed &&
	    (now.x != s_prev.x || now.y != s_prev.y))
		ev |= TOUCH_EV_MOVE;

	*ts    = now;
	s_prev = now;
	s_last = now;
	return ev;
}
