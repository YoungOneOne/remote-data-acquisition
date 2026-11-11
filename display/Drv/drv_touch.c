#include "main.h"
#include "i2c.h"
#include "drv_touch.h"

#define REG_TD_STATUS       0x02
#define REG_TOUCH1_XH       0x03
#define REG_GMODE           0xA4
#define REG_CHIP_ID         0xA8

#define TOUCH_I2C_TMO       2

static uint8_t s_chip_id;

uint8_t drv_touch_init(void)
{
	uint8_t mode = 0x00;

	s_chip_id = 0;

	if (HAL_I2C_IsDeviceReady(&hi2c3, TOUCH_I2C_ADDR, 3, 100) != HAL_OK)
		return TOUCH_ERR;

	(void)HAL_I2C_Mem_Read(&hi2c3, TOUCH_I2C_ADDR, REG_CHIP_ID,
	                       I2C_MEMADD_SIZE_8BIT, &s_chip_id, 1, 100);

	(void)HAL_I2C_Mem_Write(&hi2c3, TOUCH_I2C_ADDR, REG_GMODE,
	                        I2C_MEMADD_SIZE_8BIT, &mode, 1, 100);

	return TOUCH_OK;
}

uint8_t drv_touch_chip_id(void)
{
	return s_chip_id;
}

uint8_t drv_touch_scan(Touch_State *ts)
{
	uint8_t  buf[5];
	uint16_t raw_x, raw_y;

	if (ts == 0)
		return TOUCH_ERR;

	ts->pressed = 0;
	ts->count   = 0;

	if (HAL_I2C_Mem_Read(&hi2c3, TOUCH_I2C_ADDR, REG_TD_STATUS,
	                     I2C_MEMADD_SIZE_8BIT, buf, 5, TOUCH_I2C_TMO) != HAL_OK)
		return TOUCH_ERR;

	ts->count = (uint8_t)(buf[0] & 0x0Fu);
	if (ts->count == 0)
		return TOUCH_OK;

	raw_x = (uint16_t)(((uint16_t)(buf[1] & 0x0Fu) << 8) | buf[2]);
	raw_y = (uint16_t)(((uint16_t)(buf[3] & 0x0Fu) << 8) | buf[4]);

	ts->x = raw_y;
	ts->y = raw_x;

	if (ts->x >= TOUCH_MAX_X || ts->y >= TOUCH_MAX_Y)
		return TOUCH_OK;

	ts->pressed = 1;
	return TOUCH_OK;
}

static Touch_State s_last;

const Touch_State *drv_touch_last(void)
{
	return &s_last;
}

#define TOUCH_FAIL_MAX      3

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

		if (++s_fail < TOUCH_FAIL_MAX)
		{
			*ts = s_prev;
			return 0;
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
