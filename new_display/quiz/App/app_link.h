/**
  ******************************************************************************
  * @file    app_link.h
  * @brief   与采集端的链路管理接口
  ******************************************************************************
  */

#ifndef __APP_LINK_H
#define __APP_LINK_H

#include <stdint.h>
#include "app_gfx.h"

/* 超过这么久没收到数据就算链路断开 (单位 ms) */
#define APP_LINK_TIMEOUT_MS     500u

void    app_link_init(void);

/* 主循环轮询接口：收数据、拆帧、更新统计。返回 1 表示本轮收到新波形或控制应答，需要刷新界面 */
uint8_t app_link_poll(void);

/* 取最近一窗波形数据。返回 1 表示有效 */
uint8_t app_link_get_window(wave_window_t *w);

/* 1 = 链路在线 (最近收到数据) */
uint8_t app_link_is_alive(void);

/* ---- 下发控制指令 ---- */
void    app_link_cmd_scan(uint8_t scan);
void    app_link_cmd_sqw(uint8_t on, uint32_t freq);
void app_link_cmd_trig(uint8_t src, uint8_t edge, uint8_t mode, uint16_t level);
void    app_link_cmd_run(uint8_t run);
void    app_link_cmd_arm(void);		/* ARM：单次触发重新就绪 */
void    app_link_cmd_sync(void);		/* SYNC：请求采集端回一次完整状态 */

#endif /* __APP_LINK_H */
