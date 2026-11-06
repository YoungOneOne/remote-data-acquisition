#include "main.h"
#include "tim.h"
#include "drv_key.h"
#include "app_ui.h"
#include "app_tim.h"

void app_tim_init(void)
{
	HAL_TIM_Base_Start_IT(&htim3);
	HAL_TIM_Base_Start_IT(&htim1);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	if (htim->Instance == TIM3)
	{
		/* TIM3 节拍 (原有 PC7 调试翻转已移除，避免破坏 USART6_RX PC7 通信) */
	}

	if (htim->Instance == TIM1)
	{
		drv_key_tick();
	}
}
