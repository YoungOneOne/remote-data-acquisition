#include "main.h"
#include "tim.h"
#include "gpio.h"
#include "drv_key.h"

int8_t step=0;
uint8_t keynum=0;

uint8_t drv_key_level(void)
{
	if(HAL_GPIO_ReadPin(GPIOG, GPIO_PIN_7))
	{
		return 1;
	}
	else{
		return 0;
	}
}

uint8_t drv_key_get(void)
{
	uint8_t temp=0;
	temp=keynum;
	keynum=0;
	return temp;
}

void drv_key_tick(void)
{
	static uint16_t cnt1=0;
	static uint16_t pressdown=0;
	static uint16_t repressdown=0;
	static uint16_t pressup=0;
	static uint8_t  Curkeyval=0, Prevkeyval=0;

if(++cnt1>=20)
{
	cnt1=0;
	Prevkeyval=Curkeyval;
	Curkeyval=drv_key_level();
}

if(step==0)
{
	pressdown=0;
	pressup=0;
	repressdown=0;
	if(Curkeyval!=0&&Prevkeyval==0)
	{
		step=1;
	}
}

else if(step==1)
{
	if(Curkeyval!=0)
	{
		pressdown++;
		if(pressdown>=2000)
		{
			keynum=1;
			step=0;
		}
	}
	else{
		step=2;
	}
}

else if(step==2){
	if(++pressup<=500)
	{
		if(Curkeyval!=0)
		repressdown++;
		if(repressdown>=30)
		{
			keynum=2;
			step=0;
		}
	}
	else{
		keynum=3;
		step=0;
	}
}

}
