/**
  ******************************************************************************
  * @file    app_layout.h
  * @brief   显示端界面布局常量与配色表
  *
 *          屏幕分区、采样点数和颜色集中放在这里。
  *
  *          屏幕：RK043FN48H-CT672B  480 x 272  RGB565
  ******************************************************************************
  */

#ifndef __APP_LAYOUT_H
#define __APP_LAYOUT_H

#include <stdint.h>
#include "../../../Common/scope_protocol.h"

/* ==========================================================================
 *  1. 屏幕与分区几何
 * ========================================================================== */

#define SCR_W               480u
#define SCR_H               272u

/* 状态栏 */
#define ST_Y                0u
#define ST_H                20u

/* 波形区 */
#define WV_Y                20u
#define WV_H                207u
#define WV_BOT              (WV_Y + WV_H)       /* 227 */

/* 五路同屏：模拟 144px，数字三条各 21px，共用横向十格。 */
#define ANA_Y               WV_Y
#define ANA_H               144u
#define ANA_BOT             (ANA_Y + ANA_H)
#define ANA_DIVS            6u
#define DIG_LANES           3u
#define DIG_H               21u
#define DIG_Y               ANA_BOT

/* 底栏：18px 测量摘要 + 27px 触摸按键。 */
#define TB_Y                WV_BOT
#define TB_H                (SCR_H - TB_Y)
#define WAVE_W              SCR_W
#define DIVX                SCOPE_DIVISIONS
#define DIVY                ANA_DIVS
#define DW                  (WAVE_W / DIVX)
#define DH                  (ANA_H / ANA_DIVS)

/* 每窗口采样点数：每格 50 点 x 10 格 */
#define PPD                 SCOPE_POINTS_PER_DIV
#define NPT                 (PPD * DIVX)        /* 500 */

/* ==========================================================================
 *  2. 配色表（RGB565）
 *     括号内是 PC 端 demo 用的 CSS 十六进制，便于对照
 * ========================================================================== */

#define C_BG            0x0000u   /* #000000  波形区背景        */
#define C_GRID          0x2105u   /* #1C1C1C  细网格            */
#define C_GRID_MAJ      0x4208u   /* #333333  中轴线            */
#define C_EDGE          0x4A49u   /* #4A4A4A  区域边框          */

#define C_CH1           0xFFE0u   /* #FFE000  黄                */
#define C_CH2           0x07FFu   /* #00E0FF  青                */
#define C_DIG0            0x07E0u   /* #00E000  绿                */
#define C_DIG1            0xF81Fu   /* #FF3CFF  品红              */
#define C_DIG2            0xFD20u   /* #FF8000  橙                */
#define C_TRIG          0xF800u   /* #FF2020  触发电平线        */

#define C_ST_BG         0x1084u   /* #101820  状态栏底          */

#define C_TB_BG         0x1905u   /* #1A2028  工具条底          */
#define C_TB_BTN        0x29A8u   /* #2B3644  按钮              */
#define C_TB_BTN_ON     0x3AEFu   /* #3D5A78  按钮激活          */
#define C_TB_TX         0xFFFFu   /* #FFFFFF  按钮字            */
#define C_TB_MENU       0x3988u   /* #3A3040  设置按钮          */
#define C_TB_BTN_OFF    0x232Au   /* #232A33  通道按钮关闭      */

#define C_DIM           0x632Du   /* #606870  变暗文字          */

#define C_WHITE         0xFFFFu
#define C_BLACK         0x0000u
#define C_GREEN         0x2E08u   /* #28E070  运行 / 已触发     */
#define C_AMBER         0xFD60u   /* #FFB020  等待 / 警告       */
#define C_RED           0xF800u
#define C_BLUE          0x7D3Fu   /* #79C0FF  高亮              */

/* ==========================================================================
 *  3. 帧缓冲（放在 SDRAM，见 drv_lcd.h）
 * ========================================================================== */

#define FB_PIXELS       ((uint32_t)SCR_W * SCR_H)   /* 130560 */
#define FB_BYTES        (FB_PIXELS * 2u)            /* 261120 */

#endif /* __APP_LAYOUT_H */
