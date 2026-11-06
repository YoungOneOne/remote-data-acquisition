/**
  ******************************************************************************
  * @file    drv_debug.h
  * @brief   调试输出驱动（printf 重定向）
  *
  *          Drv 层 —— 硬件驱动层
  ******************************************************************************
  */

#ifndef __DRV_DEBUG_H
#define __DRV_DEBUG_H

/* 初始化调试输出。
 * 默认走 SWO/ITM，Keil 里用 View -> Serial Windows -> Debug (printf) Viewer 查看。
 * main() 里调用一次即可。 */
void drv_debug_init(void);

#endif /* __DRV_DEBUG_H */
