/**
  ******************************************************************************
  * @file    bsp_generator.c
  * @brief   100Hz - 10kHz 矩形波发生器驱动实现 (TIM3_CH1 / PB4 引脚)
  ******************************************************************************
  */

#include "bsp_generator.h"

// 6 个常用的预设频率档位 (单位: Hz)
const uint32_t c_generator_presets[GEN_PRESET_COUNT] = {
    100, 500, 1000, 2000, 5000, 10000
};

static uint32_t s_current_freq = GEN_FREQ_DEFAULT;
static uint8_t  s_preset_index = 2; // 默认指向 1000 Hz 档位
static uint8_t  s_is_running   = 0; // 输出使能状态

// 初始化方波发生器 (上电默认输出 1kHz, 50% 占空比方波)
void Generator_Init(void)
{
    Generator_SetFrequency(GEN_FREQ_DEFAULT);
    Generator_Start();
}

// 开启方波 PWM 输出
void Generator_Start(void)
{
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
    s_is_running = 1;
}

// 停止方波 PWM 输出
void Generator_Stop(void)
{
    HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_1);
    s_is_running = 0;
}

// 查询当前发生器是否处于开启状态
uint8_t Generator_IsEnabled(void)
{
    return s_is_running;
}

// TIM3 每微秒计数一次。周期计数 = 1000000 / 频率，高电平占半个周期。
uint32_t Generator_SetFrequency(uint32_t freq_hz)
{
    uint32_t period;
    // 边界值保护：限制在 100Hz 到 10kHz 之间
    if (freq_hz < GEN_FREQ_MIN) {
        freq_hz = GEN_FREQ_MIN;
    }
    if (freq_hz > GEN_FREQ_MAX) {
        freq_hz = GEN_FREQ_MAX;
    }

    s_current_freq = freq_hz;

    period = 1000000UL / freq_hz; // 限频后为 100～10000，能放入 16 位定时器。

    // 写入定时器寄存器
    __HAL_TIM_SET_PRESCALER(&htim3, 107);
    __HAL_TIM_SET_AUTORELOAD(&htim3, period - 1);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, period / 2);

    // 产生一次软件更新事件，促使新设置的 ARR 和 CCR 立即加载生效
    htim3.Instance->EGR = TIM_EGR_UG;

    // 如果当前处于开启状态，确保 PWM 输出持续工作
    if (s_is_running) {
        HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
    }

    // 检查是否恰好命中某个预设档位，同步预设索引
    for (uint8_t i = 0; i < GEN_PRESET_COUNT; i++) {
        if (c_generator_presets[i] == freq_hz) {
            s_preset_index = i;
            break;
        }
    }

    return s_current_freq;
}

// 获取当前发生器频率
uint32_t Generator_GetFrequency(void)
{
    return s_current_freq;
}

// 切换到下一个预设频率档位
uint32_t Generator_NextPreset(void)
{
    s_preset_index = (s_preset_index + 1) % GEN_PRESET_COUNT;
    return Generator_SetFrequency(c_generator_presets[s_preset_index]);
}

// 切换到上一个预设频率档位
uint32_t Generator_PrevPreset(void)
{
    if (s_preset_index == 0) {
        s_preset_index = GEN_PRESET_COUNT - 1;
    } else {
        s_preset_index--;
    }
    return Generator_SetFrequency(c_generator_presets[s_preset_index]);
}
