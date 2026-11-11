#ifndef __DRV_LCD_H
#define __DRV_LCD_H

#include <stdint.h>

#define SDRAM_BASE          0xC0000000u
#define SDRAM_SIZE          0x00800000u

#define FB0_ADDR            (SDRAM_BASE + 0x00000000u)
#define FB1_ADDR            (SDRAM_BASE + 0x00100000u)
#define FB2_ADDR            (SDRAM_BASE + 0x00200000u)

#define BSP_OK              0u
#define BSP_ERROR           1u

extern uint8_t g_sdram_ok;

extern uint8_t  g_sdram_stage;
extern uint32_t g_sdram_test_idx;
extern uint32_t g_sdram_test_want;
extern uint32_t g_sdram_test_got;

uint8_t drv_sdram_init(void);
uint8_t drv_sdram_test(void);
void    drv_sdram_dump(void);

uint8_t drv_lcd_init(uint16_t *fb_addr);
void    drv_lcd_set_fb(uint16_t *fb_addr);
void    drv_lcd_set_fb_nowait(uint16_t *fb_addr);
void    drv_lcd_wait_vsync(void);
void    drv_lcd_wait_reload(void);
void    drv_lcd_backlight(uint8_t on);

#endif
