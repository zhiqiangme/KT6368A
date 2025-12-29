#include "main.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
float temperature; 
char temperature_string[24];
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

int main(void)
{
	uint8_t DAT;
	
	HAL_Init();		//初始化HAL库

    sys_stm32_clock_init(RCC_PLL_MUL9);     /* 设置时钟, 72Mhz */
    delay_init(72);                         /* 延时初始化 */
    usart_init(115200);                     /* 串口初始化为115200 */
	/* Add your application code here */
	
	/*模块初始化*/
	OLED_Init();		//OLED初始化
	
	T117_Init();
	delay_ms(100);
	
	/*OLED显示*/
	OLED_ShowString(24, 0, "电子温度计", OLED_8X16);
	/* Infinite loop */
	while (1)
	{
		// 读取状态寄存器，检查温度转换是否完成
		if(T117_R_REG(0x03, &DAT) == 0)
		{
			// 检查温度转换状态位（位5）：0=完成，1=转换中
			if((DAT & 0x20) == 0)
			{
				// 温度转换完成, 读取温度值, 存入temperature
				T117_R_TEMP(&temperature);
				//sprintf(temperature_string, "温度:%5.2fC", temperature);
				//OLED_ShowString(1, 1, temperature_string, OLED_8X16);
				OLED_Printf(0, 16, OLED_8X16, "温度:%5.2fC", temperature);
			}
		}

		OLED_Printf(30, 48, OLED_8X16, "检测完成");
		OLED_Update();
		delay_ms(500);
	}
}
