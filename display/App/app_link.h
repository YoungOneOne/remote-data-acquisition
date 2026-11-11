#ifndef __APP_LINK_H
#define __APP_LINK_H

#include <stdint.h>
#include "app_gfx.h"

#define APP_LINK_TIMEOUT_MS     500u

void    app_link_init(void);

uint8_t app_link_poll(void);

uint8_t app_link_get_window(wave_window_t *w);

uint8_t app_link_is_alive(void);

void    app_link_cmd_scan(uint8_t scan);
void    app_link_cmd_sqw(uint8_t on, uint32_t freq);
void app_link_cmd_trig(uint8_t src, uint8_t edge, uint8_t mode, uint16_t level);
void    app_link_cmd_run(uint8_t run);
void    app_link_cmd_arm(void);
void    app_link_cmd_sync(void);

#endif
