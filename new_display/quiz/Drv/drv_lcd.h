/**
  ******************************************************************************
  * @file    drv_lcd.h
  * @brief   32F746G-DISCO 板级驱动：FMC SDRAM + LTDC 显示
  *
  *          时序与引脚均取自 ST 官方资料，不是自己算的：
  *            SDRAM 时序  : CubeF7 Drivers/BSP/STM32746G-Discovery/stm32746g_discovery_sdram.c
  *            FMC 引脚    : UM1907 表 11（32F746GDISCOVERY I/O assignment）
  *            面板时序    : rk043fn48h.h
  ******************************************************************************
  */

#ifndef __DRV_LCD_H
#define __DRV_LCD_H

#include <stdint.h>

/* SDRAM：MT48LC4M32B2B5-6A，FMC Bank1，板上 16 位总线 → 8 MB 可用 */
#define SDRAM_BASE          0xC0000000u
#define SDRAM_SIZE          0x00800000u        /* 8 MB */

/* 三块帧缓冲，各占 1MB 对齐空间（每帧 255KB）。
 * LTDC 显示一块、一块已提交待切换、一块供绘制，翻页互不等待。 */
#define FB0_ADDR            (SDRAM_BASE + 0x00000000u)
#define FB1_ADDR            (SDRAM_BASE + 0x00100000u)
#define FB2_ADDR            (SDRAM_BASE + 0x00200000u)

/* 返回值 */
#define BSP_OK              0u
#define BSP_ERROR           1u

/* 1 = SDRAM 已初始化且自检通过。为 0 时不要访问 0xC0000000 */
extern uint8_t g_sdram_ok;

/* 诊断信息（排查 SDRAM 用）
 *   g_sdram_stage: 0=未开始 1=HAL_SDRAM_Init 失败 2=CLK_ENABLE 失败
 *                  3=PALL 失败 4=AUTOREFRESH 失败 5=LOAD_MODE 失败
 *                  6=初始化完成（还没做读写校验）
 *                  7=读写校验失败（细节看 test_idx/want/got）8=全部通过 */
extern uint8_t  g_sdram_stage;
extern uint32_t g_sdram_test_idx;
extern uint32_t g_sdram_test_want;
extern uint32_t g_sdram_test_got;

uint8_t drv_sdram_init(void);
uint8_t drv_sdram_test(void);               /* 写读比对，返回 BSP_OK/BSP_ERROR */
void    drv_sdram_dump(void);               /* 打印 FMC 寄存器实际值（诊断）*/

uint8_t drv_lcd_init(uint16_t *fb_addr);    /* 把 LTDC 图层指向 fb_addr（立即重载）*/
void    drv_lcd_set_fb(uint16_t *fb_addr);  /* 切帧缓冲，等垂直消隐完成（会阻塞 ≤16.7ms）*/
void    drv_lcd_set_fb_nowait(uint16_t *fb_addr); /* 只提交 VBR，立刻返回 */
void    drv_lcd_wait_vsync(void);           /* 等一个垂直消隐边界（驱动刷新节拍）*/
void    drv_lcd_wait_reload(void);          /* 同上，保留旧名兼容 */
void    drv_lcd_backlight(uint8_t on);      /* PK3 背光 */

#endif
