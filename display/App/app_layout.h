#ifndef __APP_LAYOUT_H
#define __APP_LAYOUT_H

#include <stdint.h>
#include "scope_protocol.h"

#define SCR_W               480u
#define SCR_H               272u

#define ST_Y                0u
#define ST_H                20u

#define WV_Y                20u
#define WV_H                207u
#define WV_BOT              (WV_Y + WV_H)

#define ANA_Y               WV_Y
#define ANA_H               144u
#define ANA_BOT             (ANA_Y + ANA_H)
#define ANA_DIVS            6u
#define DIG_LANES           3u
#define DIG_H               21u
#define DIG_Y               ANA_BOT

#define TB_Y                WV_BOT
#define TB_H                (SCR_H - TB_Y)
#define WAVE_W              SCR_W
#define DIVX                SCOPE_DIVISIONS
#define DIVY                ANA_DIVS
#define DW                  (WAVE_W / DIVX)
#define DH                  (ANA_H / ANA_DIVS)

#define PPD                 SCOPE_POINTS_PER_DIV
#define NPT                 (PPD * DIVX)

#define OV_X                24
#define OV_Y                26
#define OV_W                (SCR_W - 48)
#define OV_H                (WV_BOT - OV_Y - 6)
#define OV_H_SCAN           116

#define C_BG            0x0000u
#define C_GRID          0x2105u
#define C_GRID_MAJ      0x4208u
#define C_EDGE          0x4A49u

#define C_CH1           0xFFE0u
#define C_CH2           0x07FFu
#define C_DIG0            0x07E0u
#define C_DIG1            0xF81Fu
#define C_DIG2            0xFD20u
#define C_TRIG          0xF800u

#define C_ST_BG         0x1084u
#define C_ST_TX         0xFFFFu

#define C_TB_BG         0x1905u
#define C_TB_BTN        0x29A8u
#define C_TB_BTN_ON     0x3AEFu
#define C_TB_TX         0xFFFFu
#define C_TB_MENU       0x3988u
#define C_TB_BTN_OFF    0x232Au

#define C_DIM           0x632Du
#define C_OV_BG         0x0883u
#define C_OV_HD         0x1D47u
#define C_MEAS          0xEF5Du

#define C_WHITE         0xFFFFu
#define C_BLACK         0x0000u
#define C_GREEN         0x2E08u
#define C_AMBER         0xFD60u
#define C_RED           0xF800u
#define C_BLUE          0x7D3Fu

#define FB_PIXELS       ((uint32_t)SCR_W * SCR_H)
#define FB_BYTES        (FB_PIXELS * 2u)

#endif
