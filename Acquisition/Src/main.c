/* USER CODE BEGIN Header */

/**
  * @file  main.c
  * @brief 远程数据采集系统 - 采集板主程序
  */

/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "bsp_acquisition.h"
#include "bsp_protocol.h"
#include "bsp_generator.h"
#include <stdio.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* 一帧占 1500 字节，放在静态区，避免占满主栈。 */
static AcqPoint_t frame_points[ACQ_FRAME_POINTS];

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// 只在串口空闲时取帧；NORMAL/SINGLE 未触发时不会发送新波形。
static void App_SendFrame(void)
{
    static uint32_t last_text_tick;
    float ch1_v, ch2_v, ch2_vpp;
    uint8_t dig_mask;

    if (Protocol_IsBusy() || !Acquisition_IsRunning()) return;
    // 文本每 100ms 最多发送一次，等待期间直接回主循环处理命令。
    if (Protocol_GetMode() == PROTOCOL_MODE_TEXT && HAL_GetTick() - last_text_tick < 100) return;
    if (!Acquisition_IsFrameReady()) return;
    if (!Acquisition_GetFrame(frame_points, &ch1_v, &ch2_v, &ch2_vpp, &dig_mask)) return;

    Protocol_OutputFrame(Acquisition_GetFrameTimebase(), frame_points, ACQ_FRAME_POINTS,
                         ch1_v, ch2_v, ch2_vpp, dig_mask, Acquisition_GetOvrErrorCount());
    last_text_tick = HAL_GetTick();
    HAL_GPIO_TogglePin(GPIOI, GPIO_PIN_1);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */
  

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC3_Init();
  MX_USART1_UART_Init();
  MX_DMA_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_USART6_UART_Init();
  /* USER CODE BEGIN 2 */

  // 1. 初始化通信与采集模块
  Protocol_Init();
  Acquisition_Init();

  // 2. 初始化方波发生器 (TIM3_CH1, PB4, 默认 1kHz, 50% 占空比)
  Generator_Init();

  // 3. 打印启动信息
  printf("\r\n[Acquisition Board Init OK]\r\n");
  printf("Firmware: WAVE500-TRIGFIX, type=02, points=500, UART=460800\r\n");
  printf("CH1: Pot (PF10), CH2: Mic (PA0)\r\n");
  printf("DIG: PI0, PF8, PI2\r\n");
  printf("PWM Generator: %u Hz (PB4)\r\n", (unsigned int)Generator_GetFrequency());
  printf("Timebase: %s\r\n\r\n", Acquisition_GetTimebaseName(Acquisition_GetTimebase()));

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  while (1)
  {
      Protocol_ProcessRx();
      App_SendFrame();
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

  }

  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  /** Configure the main internal regulator output voltage 
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  /** Initializes the CPU, AHB and APB busses clocks 
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 432;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Activate the Over-Drive mode 
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB busses clocks 
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_7) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USART1|RCC_PERIPHCLK_USART6;
  PeriphClkInitStruct.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK2;
  PeriphClkInitStruct.Usart6ClockSelection = RCC_USART6CLKSOURCE_PCLK2;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */

  printf("\r\n*** [CRITICAL ERROR] Error_Handler() triggered! ***\r\n");

  while (1)

  {

      HAL_GPIO_TogglePin(GPIOI, GPIO_PIN_1);

      for (volatile int i = 0; i < 500000; i++);

  }

  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{ 
  /* USER CODE BEGIN 6 */

  /* User can add his own implementation to report the file name and line number,

     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
