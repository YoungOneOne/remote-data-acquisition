#ifndef __DRV_TOUCH_H
#define __DRV_TOUCH_H

#include <stdint.h>

#define TOUCH_I2C_ADDR      0x70
#define TOUCH_MAX_X         480
#define TOUCH_MAX_Y         272

#define TOUCH_OK            0u
#define TOUCH_ERR           1u

#define TOUCH_EV_DOWN       0x01u
#define TOUCH_EV_UP         0x02u
#define TOUCH_EV_MOVE       0x04u

typedef struct
{
	uint8_t  pressed;
	uint16_t x;
	uint16_t y;
	uint8_t  count;
} Touch_State;

uint8_t drv_touch_init(void);

uint8_t drv_touch_chip_id(void);

uint8_t drv_touch_scan(Touch_State *ts);

uint8_t drv_touch_poll(Touch_State *ts);

const Touch_State *drv_touch_last(void);

#endif
