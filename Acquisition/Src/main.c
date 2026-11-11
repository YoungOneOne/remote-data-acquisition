/* USER CODE BEGIN Header */

// 远程数据采集系统 - 采集板主程序

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
// 波形结果放在静态区，避免占用过多栈空间
static AcqFrame_t frame;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// TIM2 控制采样节拍，ADC DMA 回调更新历史数据，串口中断保存命令字节
// 主循环负责执行命令、查找触发点和发送波形
// 串口空闲且有新波形时发送，NORMAL 和 SINGLE 未触发时继续等待
static void App_SendFrame(void)
{
    // 保存上次文本输出的时刻，不能在每次调用时重新清零
    static uint32_t last_text_tick;
    ProtocolMode_t mode = Protocol_GetMode();

    if (Protocol_IsBusy() || !Acquisition_IsRunning()) return;
    // 文本每 100 ms 最多发送一次，其余时间继续处理命令
    if (mode == PROTOCOL_MODE_TEXT && HAL_GetTick() - last_text_tick < 100) return;
    if (!Acquisition_IsFrameReady()) return; // 没有新数据时直接回到主循环
    if (!Acquisition_GetFrame(&frame)) return; // 数据复制失败或未满足触发条件时不发送

    if (mode == PROTOCOL_MODE_BINARY) {
        Protocol_SendBinaryFrame(frame.timebase, frame.points);
    } else {
        // 文本和二进制使用同一份取帧结果，只是发送格式不同
        Protocol_PrintDebugText(frame.timebase, frame.ch1_voltage, frame.ch2_voltage,
                                frame.ch2_vpp, frame.digital, Acquisition_GetOvrErrorCount());
    }
    last_text_tick = HAL_GetTick();
    HAL_GPIO_TogglePin(GPIOI, GPIO_PIN_1); // 每发出一帧翻转一次，用来观察输出活动
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
  // DMA 必须先初始化，后面的 ADC 和串口初始化会用到它
  MX_DMA_Init();
  MX_ADC3_Init();
  MX_USART1_UART_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_USART6_UART_Init();
  /* USER CODE BEGIN 2 */

  // 开启串口接收和采样
  Protocol_Init();
  // 先开启串口接收，后面采样启动后就可以响应控制命令
  Acquisition_Init();

  // 启动默认测试方波
  Generator_Init();

  // 打印启动信息
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
      // 先处理命令，再决定要不要发送，暂停和等待触发时也能响应操作
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
