/* Desktop substitutes for the ADC/GPIO/timer registers used by the real C code. */
#pragma once
#include <stdint.h>

typedef struct { uint32_t SR, ARR, CNT, EGR, PSC, CCR1, DIER; } TIM_TypeDef;
typedef struct { TIM_TypeDef *Instance; } TIM_HandleTypeDef;
typedef struct { uint32_t IDR, ODR; } GPIO_TypeDef;
typedef struct { uint32_t Pin, Mode, Pull, Speed; } GPIO_InitTypeDef;
typedef struct { int Instance; } ADC_HandleTypeDef;
extern TIM_TypeDef test_timer2, test_timer3;
extern TIM_HandleTypeDef htim2, htim3;
extern GPIO_TypeDef test_gpiof, test_gpiog, test_gpioi;
extern ADC_HandleTypeDef hadc3;

#define TIM2 (&test_timer2)
#define GPIOF (&test_gpiof)
#define GPIOG (&test_gpiog)
#define GPIOI (&test_gpioi)
#define ADC3 3
#define TIM2_IRQn 20
#define TIM_CHANNEL_1 1
#define TIM_IT_UPDATE 1u
#define TIM_FLAG_UPDATE 1u
#define TIM_SR_UIF 1u
#define TIM_EGR_UG 1u
#define ADC_FLAG_OVR 1u
#define GPIO_PIN_0 1u
#define GPIO_PIN_1 2u
#define GPIO_PIN_2 4u
#define GPIO_PIN_7 128u
#define GPIO_PIN_8 256u
#define GPIO_PIN_SET 1
#define GPIO_PIN_RESET 0
#define GPIO_MODE_OUTPUT_PP 1
#define GPIO_NOPULL 0
#define GPIO_SPEED_FREQ_LOW 0
#define __HAL_RCC_GPIOG_CLK_ENABLE() ((void)0)
#define __HAL_RCC_DAC_CLK_ENABLE() ((void)0)
#define __HAL_TIM_SET_AUTORELOAD(h, value) ((h)->Instance->ARR = (value))
#define __HAL_TIM_SET_COUNTER(h, value) ((h)->Instance->CNT = (value))
#define __HAL_TIM_SET_PRESCALER(h, value) ((h)->Instance->PSC = (value))
#define __HAL_TIM_SET_COMPARE(h, channel, value) ((h)->Instance->CCR1 = (value))
#define __HAL_TIM_CLEAR_FLAG(h, flag) ((h)->Instance->SR &= ~(flag))
#define __HAL_TIM_ENABLE_IT(h, flag) ((h)->Instance->DIER |= (flag))
#define __HAL_ADC_CLEAR_FLAG(h, flag) ((void)0)
#define __DMB() test_memory_barrier()

void test_memory_barrier(void);
void HAL_GPIO_Init(GPIO_TypeDef *, GPIO_InitTypeDef *);
void HAL_GPIO_WritePin(GPIO_TypeDef *, uint32_t, int);
int HAL_ADC_Start_DMA(ADC_HandleTypeDef *, uint32_t *, uint32_t);
int HAL_ADC_Stop_DMA(ADC_HandleTypeDef *);
int HAL_TIM_Base_Start(TIM_HandleTypeDef *);
int HAL_TIM_Base_Stop(TIM_HandleTypeDef *);
int HAL_TIM_PWM_Start(TIM_HandleTypeDef *, int);
int HAL_TIM_PWM_Stop(TIM_HandleTypeDef *, int);
