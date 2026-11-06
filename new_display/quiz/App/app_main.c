/**
  ******************************************************************************
  * @file    app_main.c
  * @brief   显示端入口：初始化、接收数据、处理触摸、刷新界面
  ******************************************************************************
  */
#include "main.h"
#include "app_main.h"
#include "app_gfx.h"
#include "app_ui.h"
#include "app_link.h"
#include "app_tim.h"
#include "drv_debug.h"
#include "drv_lcd.h"
#include "drv_mpu.h"
#include "drv_touch.h"
#include "drv_uart.h"
#include <stdio.h>
#include <string.h>

/* 是否开机自检 SDRAM 并启用三缓冲。
 *   1 = 启用：LTDC 显示一块、CPU 画另一块、翻页切换 -> 彻底没有撕裂
 *   0 = 退回内部 SRAM 单缓冲：一定有撕裂带（就是那条往下扫的灰带）
 *
 * 自检失败会自动退回单缓冲，不会白屏。
 * 想临时排除 SDRAM 因素，把这里改成 0 重烧即可。 */
#define APP_SDRAM_ENABLE    1

static Touch_State s_ts;            /* 触摸状态 */
static uint8_t     s_touch_ok;      /* 1 = FT5336 在线 */
static uint8_t     s_prev_pressed;  /* 上一次的按下状态，用来判边沿 */

#if APP_SDRAM_ENABLE
/* --------------------------------------------------------------------------
 *  SDRAM 初始化 + 读写自检
 *
 *  必须早于 ui_init()：ui_init() 靠 g_sdram_ok 决定用三缓冲还是单缓冲。
 *  自检会扫 3MB（覆盖 FB0/FB1/FB2 三块帧缓冲的实际位置），
 *  每段前打一行日志 —— 万一某段触发总线错误，日志就停在那一行。
 * -------------------------------------------------------------------------- */
static void app_sdram_boot(void)
{
    uint8_t ini;
    uint8_t tst = BSP_ERROR;

    ini = drv_sdram_init();
    printf("boot: sdram_init=%s stage=%d\r\n",
           (ini == BSP_OK) ? "OK" : "FAIL", (int)g_sdram_stage);
    drv_sdram_dump();

    if (ini == BSP_OK)
    {
        tst = drv_sdram_test();
        printf("boot: sdram_test=%s stage=%d\r\n",
               (tst == BSP_OK) ? "OK" : "FAIL", (int)g_sdram_stage);

        if (tst != BSP_OK)
            printf("      first bad off=0x%06lX want=0x%08lX got=0x%08lX\r\n",
                   (unsigned long)g_sdram_test_idx,
                   (unsigned long)g_sdram_test_want,
                   (unsigned long)g_sdram_test_got);
    }

    if (ini == BSP_OK && tst == BSP_OK)
        printf("boot: SDRAM OK -> 3-buffer @C0000000\r\n");
    else
        printf("boot: SDRAM FAIL -> fallback to SRAM single buffer\r\n");
}
#endif

void app_init(void)
{
    drv_mpu_config();
    SCB_EnableICache();

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;

    app_tim_init();                 /* 定时器中断：按键扫描节拍 */
    drv_debug_init();

#if APP_SDRAM_ENABLE
    app_sdram_boot();               /* 必须在 ui_init() 之前：它决定用几块缓冲 */
#endif

    ui_init();                      /* 显示初始化 */
    app_ui_init();                  /* 设置界面默认值 */

    s_touch_ok = (drv_touch_init() == TOUCH_OK) ? 1u : 0u;

    /* 启动 USART6 链路 DMA 循环接收 */
    drv_uart_init(DRV_UART_BAUD_DEFAULT);
    app_link_init();
    printf("Display link: WAVE500, type=02, 1513 bytes, USART6=460800\r\n");
}

/* 还没收到过任何数据时用的一窗基线（中轴 2048 = 1.65V，数字通道全低） */
static void baseline_window(wave_window_t *w)
{
    int k;

    memset(w, 0, sizeof(*w));
    w->n         = NPT;
    w->scan_code = g_ui.scan;
    w->dt_ns     = ui_scan_div_ns(w->scan_code) / PPD;

    for (k = 0; k < NPT; k++)
    {
        w->ch1[k] = 2048;
        w->ch2[k] = 2048;
        w->dig[k] = 0;
    }
}

/* 只在按下、抬起时刷新；一次按下只执行一次底栏动作。 */
static uint8_t app_poll_touch(void)
{
    uint8_t changed;
    if (!s_touch_ok) return 0;
    drv_touch_poll(&s_ts);
    ui_touch_set(s_ts.x, s_ts.y, s_ts.pressed);
    changed = s_ts.pressed != s_prev_pressed;
    if (s_ts.pressed && !s_prev_pressed) {
        int button = ui_toolbar_hit(s_ts.x, s_ts.y);
        if (button != UI_WID_NONE) app_ui_widget(button);
    }
    s_prev_pressed = s_ts.pressed;
    return changed;
}

void app_task(void)
{
    static wave_window_t window;
    static uint8_t drawn;
    static uint8_t last_link_ok;
    uint8_t touch_changed = app_poll_touch();
    uint8_t data_changed = app_link_poll();
    uint8_t redraw = !drawn || touch_changed;

    if (ui_page_get() == UI_PAGE_WAVE) {
        if (data_changed || last_link_ok != g_ui.link_ok) redraw = 1;
    } else if (app_ui_needs_refresh()) {
        redraw = 1;
    }
    last_link_ok = g_ui.link_ok;

    if (!redraw) return;
    // 没收过波形时画基线；暂停或断线后继续显示最后一帧。
    if (!app_link_get_window(&window)) baseline_window(&window);
    ui_draw(&window);
    drawn = 1;
}
