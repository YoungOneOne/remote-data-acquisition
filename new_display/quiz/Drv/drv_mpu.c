/**
  ******************************************************************************
  * @file    drv_mpu.c
  * @brief   MPU 区域配置
  *
  *          Drv 层 —— 硬件驱动层
  *
  *          默认把整片 4GB 划成禁止访问，再单独开放三块：
  *            region0  0x00000000  4GB    禁止访问（除下面两块，以及代码/SRAM 子区域）
  *            region1  0xC0000000  8MB    SDRAM 数据区
  *            region2  0xA0000000  8KB    FMC 控制寄存器
  ******************************************************************************
  */
#include "main.h"
#include "drv_mpu.h"

void drv_mpu_config(void)
{
	MPU_Region_InitTypeDef MPU_InitStruct = {0};

	HAL_MPU_Disable();

	/* 默认区域：整片 4GB 禁止访问 */
	MPU_InitStruct.Enable           = MPU_REGION_ENABLE;
	MPU_InitStruct.Number           = MPU_REGION_NUMBER0;
	MPU_InitStruct.BaseAddress      = 0x0;
	MPU_InitStruct.Size             = MPU_REGION_SIZE_4GB;
	MPU_InitStruct.SubRegionDisable = 0x87;
	MPU_InitStruct.TypeExtField     = MPU_TEX_LEVEL0;
	MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
	MPU_InitStruct.DisableExec      = MPU_INSTRUCTION_ACCESS_DISABLE;
	MPU_InitStruct.IsShareable      = MPU_ACCESS_SHAREABLE;
	MPU_InitStruct.IsCacheable      = MPU_ACCESS_NOT_CACHEABLE;
	MPU_InitStruct.IsBufferable     = MPU_ACCESS_NOT_BUFFERABLE;
	HAL_MPU_ConfigRegion(&MPU_InitStruct);

	/* SDRAM 数据区：region0 把它划成了 NO_ACCESS，这里单独开放 */
	MPU_InitStruct.Enable           = MPU_REGION_ENABLE;
	MPU_InitStruct.Number           = MPU_REGION_NUMBER1;
	MPU_InitStruct.BaseAddress      = 0xC0000000;
	MPU_InitStruct.Size             = MPU_REGION_SIZE_8MB;
	MPU_InitStruct.SubRegionDisable = 0x00;
	MPU_InitStruct.TypeExtField     = MPU_TEX_LEVEL0;
	MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
	MPU_InitStruct.DisableExec      = MPU_INSTRUCTION_ACCESS_DISABLE;
	MPU_InitStruct.IsShareable      = MPU_ACCESS_NOT_SHAREABLE;
	MPU_InitStruct.IsCacheable      = MPU_ACCESS_NOT_CACHEABLE;
	MPU_InitStruct.IsBufferable     = MPU_ACCESS_NOT_BUFFERABLE;
	HAL_MPU_ConfigRegion(&MPU_InitStruct);

	/* FMC 控制寄存器（0xA0000000 起 8KB）必须开放。
	 * HAL_SDRAM_Init / HAL_SDRAM_SendCommand 都在写 FMC_Bank5_6 里的寄存器，
	 * region0 把这片划成 NO_ACCESS 的话，配置 SDRAM 的第一步就会触发总线错误。
	 * 参数取自 ST 官方例程 FMC_SDRAM/Src/main.c。 */
	MPU_InitStruct.Enable           = MPU_REGION_ENABLE;
	MPU_InitStruct.Number           = MPU_REGION_NUMBER2;
	MPU_InitStruct.BaseAddress      = 0xA0000000;
	MPU_InitStruct.Size             = MPU_REGION_SIZE_8KB;
	MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
	MPU_InitStruct.IsBufferable     = MPU_ACCESS_BUFFERABLE;
	MPU_InitStruct.IsCacheable      = MPU_ACCESS_NOT_CACHEABLE;
	MPU_InitStruct.IsShareable      = MPU_ACCESS_SHAREABLE;
	MPU_InitStruct.TypeExtField     = MPU_TEX_LEVEL0;
	MPU_InitStruct.SubRegionDisable = 0x0;
	MPU_InitStruct.DisableExec      = MPU_INSTRUCTION_ACCESS_DISABLE;
	HAL_MPU_ConfigRegion(&MPU_InitStruct);

	HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}
