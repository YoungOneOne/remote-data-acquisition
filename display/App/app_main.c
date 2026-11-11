/* app_main.c：程序入口：初始化流程与主循环 */

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

#define APP_SDRAM_ENABLE    1

static Touch_State s_ts;
static uint8_t     s_touch_ok;
static uint8_t     s_prev_pressed;

#if APP_SDRAM_ENABLE

static void app_sdram_boot(void)
{
    uint8_t ini;
    uint8_t tst = BSP_ERROR;

    ini = drv_sdram_init();
    if (ini == BSP_OK) {
        printf("boot: sdram_init=OK stage=%d\r\n", (int)g_sdram_stage);
    } else {
        printf("boot: sdram_init=FAIL stage=%d\r\n", (int)g_sdram_stage);
    }
    drv_sdram_dump();

    if (ini == BSP_OK)
    {
        tst = drv_sdram_test();
        if (tst == BSP_OK) {
            printf("boot: sdram_test=OK stage=%d\r\n", (int)g_sdram_stage);
        } else {
            printf("boot: sdram_test=FAIL stage=%d\r\n", (int)g_sdram_stage);
        }

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

    app_tim_init();
    drv_debug_init();

#if APP_SDRAM_ENABLE
    app_sdram_boot();
#endif

    ui_init();
    ui_splash();
    HAL_Delay(2000);
    app_ui_init();

    if (drv_touch_init() == TOUCH_OK) {
        s_touch_ok = 1u;
    } else {
        s_touch_ok = 0u;
    }

    drv_uart_init(DRV_UART_BAUD_DEFAULT);
    app_link_init();
    printf("Display link: WAVE500, type=02, 1513 bytes, USART6=460800\r\n");
}

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

static void touch_task(void)
{
    if (!s_touch_ok)
        return;

    drv_touch_poll(&s_ts);

    if (s_ts.pressed != 0) {
        ui_touch_set((int)s_ts.x, (int)s_ts.y, 1);
    } else {
        ui_touch_set(-1, -1, 0);
    }

    s_prev_pressed = s_ts.pressed;
}

static void wave_task(wave_window_t *w)
{
    if (!app_link_get_window(w)) {
        baseline_window(w);
    }
}

void app_task(void)
{
    static wave_window_t w;

    touch_task();
    app_link_poll();
    wave_task(&w);
    ui_draw(&w);
}
