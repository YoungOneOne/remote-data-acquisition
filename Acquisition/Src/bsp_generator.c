// TIM3_CH1 输出测试方波，输出引脚为 PB4

#include "bsp_generator.h"

static uint32_t s_current_freq = GEN_FREQ_DEFAULT;
static uint8_t  s_is_running   = 0; // 输出使能状态
static uint8_t duty_percent = 50; // 占空比，单位%

// 上电输出 1 kHz 方波，默认占空比 50%
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

// TIM3 计数频率为 1 MHz，按频率和占空比设置 ARR、CCR
uint32_t Generator_SetFrequency(uint32_t freq_hz)
{
    uint32_t period;
    // 先限频再做除法，避免频率为 0 或超出定时器的计数范围
    // 生成的PWM波频率在 100Hz 到 10kHz 之间
    if (freq_hz < GEN_FREQ_MIN)
    {
        freq_hz = GEN_FREQ_MIN;
    }
    if (freq_hz > GEN_FREQ_MAX)
    {
        freq_hz = GEN_FREQ_MAX;
    }

    s_current_freq = freq_hz;

    // 计数时钟为 1 MHz，用 1000000 除以频率得到一个周期的计数次数
    period = 1000000UL / freq_hz; // 周期计数范围为 100～10000

    // 写入定时器寄存器
    // ARR 从 0 开始计数，所以减 1；CCR 决定高电平持续多少次计数
    __HAL_TIM_SET_AUTORELOAD(&htim3, period - 1);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, period * duty_percent/ 100);
    // 两个寄存器都是整数，实际频率和占空比可能有少量取整误差

    // 产生更新事件，加载新的周期和比较值
    htim3.Instance->EGR = TIM_EGR_UG;

    // 原来在输出时继续保持输出
    if (s_is_running)
    {
        HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
    }

    return s_current_freq; // 返回限频后的设定值，供命令回复和状态查询使用
}

// 获取当前发生器频率
uint32_t Generator_GetFrequency(void)
{
    return s_current_freq;
}
