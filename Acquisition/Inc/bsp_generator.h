// 测试方波的频率范围和函数声明
// 输出引脚: PB4 (Arduino D3, 扩展板 LY-AS001 插座【4-2】)
// 硬件外设: TIM3_CH1 (PWM 模式 1)

#ifndef __BSP_GENERATOR_H
#define __BSP_GENERATOR_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "main.h"
#include "tim.h"

// 频率范围和默认值
#define GEN_FREQ_MIN 100      // 最小频率: 100 Hz
#define GEN_FREQ_MAX 10000    // 最大频率: 10 kHz
#define GEN_FREQ_DEFAULT 1000 // 默认上电频率: 1 kHz

    // 方波控制接口
    void Generator_Init(void);
    void Generator_Start(void);
    void Generator_Stop(void);

    uint32_t Generator_SetFrequency(uint32_t freq_hz);
    uint32_t Generator_GetFrequency(void);

#ifdef __cplusplus
}
#endif

#endif // __BSP_GENERATOR_H
