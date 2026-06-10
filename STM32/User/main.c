#include "main.h"
#include <stdio.h>
#include <string.h>

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
float temperature;
char temperature_string[24];
extern UART_HandleTypeDef g_uart1_handle;  /* 串口句柄，用于发送温度数据到蓝牙模块 */
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

int main(void)
{
	uint8_t DAT;

	HAL_Init();		/* 初始化HAL库 */

    sys_stm32_clock_init(RCC_PLL_MUL9);     /* 设置系统时钟, 72Mhz */
    delay_init(72);                         /* 延时初始化 */
    usart_init(115200);                     /* 串口初始化为115200 */

	/* 模块初始化 */
	OLED_Init();		/* OLED初始化 */

	if(T117_Init() != 0)
	{
		OLED_ShowString(0, 16, "T117 Init Err", OLED_8X16);
		OLED_Update();
		while(1);  /* 初始化失败，停机 */
	}
	delay_ms(100);

	/* OLED显示标题 */
	OLED_ShowString(20, 0, "Thermometer", OLED_8X16);

	/* 主循环 */
	while (1)
	{
		/* 读取状态寄存器，检查温度转换是否完成 */
		if(T117_R_REG(0x03, &DAT) == 0)
		{
			/* 检查温度转换状态位（位5：0=完成，1=转换中） */
			if((DAT & 0x20) == 0)
			{
				/* 温度转换完成，读取温度值，存入temperature */
				if(T117_R_TEMP(&temperature) == 0)
				{
					OLED_ClearArea(0, 16, 128, 16);  /* 清除旧显示，防止残影 */
					OLED_Printf(0, 16, OLED_8X16, "Temp:%5.2fC", temperature);

					/* 通过串口发送温度数据到 KT6368A 蓝牙模块 */
					int len = sprintf(temperature_string, "temp=%.2fC\r\n", temperature);
					HAL_UART_Transmit(&g_uart1_handle, (uint8_t *)temperature_string, len, 100);
				}
			}
		}

		OLED_Update();
		delay_ms(500);
	}
}
