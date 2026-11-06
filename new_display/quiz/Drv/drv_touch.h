/**
  ******************************************************************************
  * @file    touch.h
  * @brief   板载电容触摸屏 FT5336 驱动（I2C3: PH7=SCL, PH8=SDA）
  *
  *          硬件事实：
  *            芯片      FT5336（FocalTech 电容触摸控制器）
  *            总线      I2C3，AF4，地址 7 位 0x38 -> HAL 传 0x70
  *            中断脚    PI13（本驱动用轮询，不用中断）
  *            分辨率    480 x 272，面板出厂是竖屏装的，所以读出来 X/Y 要交换
  *
  *          寄存器（详见备考笔记 §4.10）：
  *            0x02  TD_STATUS   低 4 位 = 触摸点数（0 = 松开）
  *            0x03  TOUCH1_XH   bit3~0 = X[11:8]（bit7~6 是事件标志，要屏蔽）
  *            0x04  TOUCH1_XL   X[7:0]
  *            0x05  TOUCH1_YH   bit3~0 = Y[11:8]
  *            0x06  TOUCH1_YL   Y[7:0]
  *            0xA4  GMODE       0 = 轮询模式
  *            0xA8  CHIP_ID
  ******************************************************************************
  */

#ifndef __DRV_TOUCH_H
#define __DRV_TOUCH_H

#include <stdint.h>

#define TOUCH_I2C_ADDR      0x70        /* FT5336 的 7 位地址 0x38 左移一位 */
#define TOUCH_MAX_X         480
#define TOUCH_MAX_Y         272

/* 返回值 */
#define TOUCH_OK            0u
#define TOUCH_ERR           1u

/* 事件位（drv_touch_poll 返回） */
#define TOUCH_EV_DOWN       0x01u       /* 刚按下 */
#define TOUCH_EV_UP         0x02u       /* 刚抬起 */
#define TOUCH_EV_MOVE       0x04u       /* 按住并移动了 */

typedef struct
{
	uint8_t  pressed;       /* 1 = 手指在屏上，0 = 松开 */
	uint16_t x;             /* 0 ~ 479，已交换成横屏方向 */
	uint16_t y;             /* 0 ~ 271 */
	uint8_t  count;         /* 触摸点数 0~5 */
} Touch_State;

/* 初始化：确认器件在线并切到轮询模式。返回 TOUCH_OK / TOUCH_ERR */
uint8_t drv_touch_init(void);

/* 读芯片 ID（调试用，读到 0 说明 I2C 没通） */
uint8_t drv_touch_chip_id(void);

/* 读一次原始状态（不做防抖）。返回 TOUCH_OK / TOUCH_ERR */
uint8_t drv_touch_scan(Touch_State *ts);

/* 常用接口：读一次 + 边沿检测。
 *   ts   : 输出当前状态
 *   返回 : TOUCH_EV_* 的位组合（0 表示"没变化"）
 *   用法 : 主循环里每次调一次（25~100Hz 都行）*/
uint8_t drv_touch_poll(Touch_State *ts);

/* 读回最近一次 drv_touch_poll() 的结果（只读指针，不用自己再读 I2C）。
 * 界面层想显示"当前触摸坐标"就用这个。 */
const Touch_State *drv_touch_last(void);

#endif /* __DRV_TOUCH_H */
