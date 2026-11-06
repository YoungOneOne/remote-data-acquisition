/* Exercise the actual timer calculation, including non-preset frequencies. */
#include <assert.h>
#include <stdio.h>
#include "bsp_generator.c"

TIM_TypeDef test_timer2, test_timer3;
TIM_HandleTypeDef htim2 = {&test_timer2}, htim3 = {&test_timer3};
int HAL_TIM_PWM_Start(TIM_HandleTypeDef *timer, int channel) { return 0; }
int HAL_TIM_PWM_Stop(TIM_HandleTypeDef *timer, int channel) { return 0; }

int main(void)
{
    Generator_Init();
    assert(Generator_IsEnabled() && Generator_GetFrequency() == 1000);
    for (uint32_t requested = 0; requested <= 11000; requested++) {
        uint32_t limited = requested < 100 ? 100 : requested > 10000 ? 10000 : requested;
        assert(Generator_SetFrequency(requested) == limited);
        assert(test_timer3.PSC == 107 && test_timer3.ARR == 1000000 / limited - 1);
        assert(test_timer3.CCR1 == (test_timer3.ARR + 1) / 2);
    }
    Generator_SetFrequency(1000);
    for (int i = 0; i < GEN_PRESET_COUNT; i++) Generator_NextPreset();
    assert(Generator_GetFrequency() == 1000);
    for (int i = 0; i < GEN_PRESET_COUNT; i++) Generator_PrevPreset();
    assert(Generator_GetFrequency() == 1000);
    Generator_SetFrequency(1234);
    assert(Generator_NextPreset() == 2000);
    Generator_Stop(); assert(!Generator_IsEnabled());
    Generator_SetFrequency(100); assert(!Generator_IsEnabled());
    Generator_Start(); assert(Generator_IsEnabled());
    puts("PASS: 11,001 frequency requests, timer registers, presets and output control");
    return 0;
}
