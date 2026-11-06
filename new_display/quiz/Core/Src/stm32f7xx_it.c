/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32f7xx_it.c
  * @brief   Interrupt Service Routines.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32f7xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>			/* 死机诊断要打印 */
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* ---------------------------------------------------------------------------
 *  死机诊断（黑匣子）
 *
 *  出错时 CPU 会自动把 8 个字压进出错那一刻的栈：
 *      [0]=R0 [1]=R1 [2]=R2 [3]=R3 [4]=R12 [5]=LR [6]=PC [7]=xPSR
 *
 *  为什么必须用 naked（裸函数）：普通 C 函数一进函数体，编译器会先 push
 *  寄存器、再减 SP；等我们在 C 里读 MSP 时，SP 已经不在出错那一刻的位置，
 *  读出来的是错的帧。裸函数里全是手写汇编，取 SP 之前一个字都不碰。
 * --------------------------------------------------------------------------- */
void fault_report(uint32_t *f);

#if defined(__CC_ARM) && !defined(__clang__)
__asm void HardFault_Handler(void)
{
    IMPORT fault_report
    TST LR, #4
    ITE EQ
    MRSEQ R0, MSP
    MRSNE R0, PSP
    B fault_report
}
#else
__attribute__((naked)) void HardFault_Handler(void)
{
  __asm volatile (
    "tst   lr, #4        \n"     /* EXC_RETURN bit2 = 0 -> 出错时用的是 MSP */
    "ite   eq            \n"
    "mrseq r0, msp       \n"
    "mrsne r0, psp       \n"
    "b     fault_report  \n"
  );
}
#endif

void fault_report(uint32_t *f)
{
  printf("\r\n!! FAULT pc=%08lX lr=%08lX psr=%08lX\r\n",
         (unsigned long)f[6], (unsigned long)f[5], (unsigned long)f[7]);
  printf("        r0=%08lX r1=%08lX r2=%08lX r3=%08lX r12=%08lX\r\n",
         (unsigned long)f[0], (unsigned long)f[1], (unsigned long)f[2],
         (unsigned long)f[3], (unsigned long)f[4]);

  /* CFSR 的每一位都说明了原因，比 PC 更可靠：
   *   bit7  MMARVALID  -> MPU 越权，MMFAR 就是那个地址
   *   bit9  PRECISERR  -> 精确总线错误
   *   bit10 IMPRECISERR-> 写错误延迟上报（写错地址时常见）
   *   bit15 BFARVALID  -> BFAR 有效，就是出错地址
   *   bit24 UNALIGNED / bit16 UNDEFINSTR -> 非对齐 / 非法指令 */
  printf("        CFSR=%08lX HFSR=%08lX MMFAR=%08lX BFAR=%08lX\r\n",
         (unsigned long)SCB->CFSR, (unsigned long)SCB->HFSR,
         (unsigned long)SCB->MMFAR, (unsigned long)SCB->BFAR);

  /* 顺便把 MSP 换算成"距栈底还有多少"：如果 SP 很低，说明栈溢出 */
  {
    uint32_t sp;
    sp = __get_MSP();
    printf("        MSP=%08lX\r\n", (unsigned long)sp);
  }

  SCB->CFSR = 0xFFFFFFFFu;
  while (1) { }
}

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim3;
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart6;
/* USER CODE BEGIN EV */

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex-M7 Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */

  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
   while (1)
  {
  }
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/* HardFault_Handler 已挪到上面 USER CODE BEGIN 0（裸函数版），这里不再定义。 */

/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler(void)
{
  /* USER CODE BEGIN MemoryManagement_IRQn 0 */

  /* USER CODE END MemoryManagement_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_MemoryManagement_IRQn 0 */
    /* USER CODE END W1_MemoryManagement_IRQn 0 */
  }
}

/**
  * @brief This function handles Pre-fetch fault, memory access fault.
  */
void BusFault_Handler(void)
{
  /* USER CODE BEGIN BusFault_IRQn 0 */

  /* USER CODE END BusFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_BusFault_IRQn 0 */
    /* USER CODE END W1_BusFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void)
{
  /* USER CODE BEGIN UsageFault_IRQn 0 */

  /* USER CODE END UsageFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_UsageFault_IRQn 0 */
    /* USER CODE END W1_UsageFault_IRQn 0 */
  }
}

/**
  * @brief This function handles System service call via SWI instruction.
  */
void SVC_Handler(void)
{
  /* USER CODE BEGIN SVCall_IRQn 0 */

  /* USER CODE END SVCall_IRQn 0 */
  /* USER CODE BEGIN SVCall_IRQn 1 */

  /* USER CODE END SVCall_IRQn 1 */
}

/**
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{
  /* USER CODE BEGIN DebugMonitor_IRQn 0 */

  /* USER CODE END DebugMonitor_IRQn 0 */
  /* USER CODE BEGIN DebugMonitor_IRQn 1 */

  /* USER CODE END DebugMonitor_IRQn 1 */
}

/**
  * @brief This function handles Pendable request for system service.
  */
void PendSV_Handler(void)
{
  /* USER CODE BEGIN PendSV_IRQn 0 */

  /* USER CODE END PendSV_IRQn 0 */
  /* USER CODE BEGIN PendSV_IRQn 1 */

  /* USER CODE END PendSV_IRQn 1 */
}

/**
  * @brief This function handles System tick timer.
  */
void SysTick_Handler(void)
{
  /* USER CODE BEGIN SysTick_IRQn 0 */

  /* USER CODE END SysTick_IRQn 0 */
  HAL_IncTick();
  /* USER CODE BEGIN SysTick_IRQn 1 */

  /* USER CODE END SysTick_IRQn 1 */
}

/******************************************************************************/
/* STM32F7xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32f7xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles TIM1 break interrupt and TIM9 global interrupt.
  */
void TIM1_BRK_TIM9_IRQHandler(void)
{
  /* USER CODE BEGIN TIM1_BRK_TIM9_IRQn 0 */

  /* USER CODE END TIM1_BRK_TIM9_IRQn 0 */
  HAL_TIM_IRQHandler(&htim1);
  /* USER CODE BEGIN TIM1_BRK_TIM9_IRQn 1 */

  /* USER CODE END TIM1_BRK_TIM9_IRQn 1 */
}

/**
  * @brief This function handles TIM1 update interrupt and TIM10 global interrupt.
  */
void TIM1_UP_TIM10_IRQHandler(void)
{
  /* USER CODE BEGIN TIM1_UP_TIM10_IRQn 0 */

  /* USER CODE END TIM1_UP_TIM10_IRQn 0 */
  HAL_TIM_IRQHandler(&htim1);
  /* USER CODE BEGIN TIM1_UP_TIM10_IRQn 1 */

  /* USER CODE END TIM1_UP_TIM10_IRQn 1 */
}

/**
  * @brief This function handles TIM3 global interrupt.
  */
void TIM3_IRQHandler(void)
{
  /* USER CODE BEGIN TIM3_IRQn 0 */

  /* USER CODE END TIM3_IRQn 0 */
  HAL_TIM_IRQHandler(&htim3);
  /* USER CODE BEGIN TIM3_IRQn 1 */

  /* USER CODE END TIM3_IRQn 1 */
}

/**
  * @brief This function handles USART1 global interrupt.
  */
void USART1_IRQHandler(void)
{
  /* USER CODE BEGIN USART1_IRQn 0 */

  /* USER CODE END USART1_IRQn 0 */
  HAL_UART_IRQHandler(&huart1);
  /* USER CODE BEGIN USART1_IRQn 1 */

  /* USER CODE END USART1_IRQn 1 */
}

/**
  * @brief This function handles USART6 global interrupt.
  */
void USART6_IRQHandler(void)
{
  /* USER CODE BEGIN USART6_IRQn 0 */

  /* USER CODE END USART6_IRQn 0 */
  HAL_UART_IRQHandler(&huart6);
  /* USER CODE BEGIN USART6_IRQn 1 */

  /* USER CODE END USART6_IRQn 1 */
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
