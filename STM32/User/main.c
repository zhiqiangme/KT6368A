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
	
	HAL_Init();		//��ʼ��HAL��

    sys_stm32_clock_init(RCC_PLL_MUL9);     /* ����ʱ��, 72Mhz */
    delay_init(72);                         /* ��ʱ��ʼ�� */
    usart_init(115200);                     /* ���ڳ�ʼ��Ϊ115200 */
	/* Add your application code here */
	
	/*ģ���ʼ��*/
	OLED_Init();		//OLED��ʼ��
	
	T117_Init();
	delay_ms(100);
	
	/*OLED��ʾ*/
	OLED_ShowString(24, 0, "�����¶ȼ�", OLED_8X16);
	/* Infinite loop */
	while (1)
	{
		// ��ȡ״̬�Ĵ���������¶�ת���Ƿ����
		if(T117_R_REG(0x03, &DAT) == 0)
		{
			// ����¶�ת��״̬λ��λ5����0=��ɣ�1=ת����
			if((DAT & 0x20) == 0)
			{
				// �¶�ת�����, ��ȡ�¶�ֵ, ����temperature
				T117_R_TEMP(&temperature);
				OLED_ClearArea(0, 16, 128, 16);  // �����ɵ�ʾ����ֹ����
				OLED_Printf(0, 16, OLED_8X16, "Temp:%5.2fC", temperature);
			}
		}

		OLED_Update();
		delay_ms(500);
	}
}
