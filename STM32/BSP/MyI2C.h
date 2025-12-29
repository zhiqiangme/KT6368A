#ifndef __MYI2C_H
#define __MYI2C_H

#include "stm32f1xx_hal.h"  // 修改这里
#include "delay.h"

/* 引脚定义 */
#define MyI2C_SCL_PIN GPIO_PIN_10
#define MyI2C_SDA_PIN GPIO_PIN_11
#define MyI2C_GPIO_PORT GPIOB

/* 函数声明 */
void MyI2C_Init(void);
void MyI2C_Start(void);
void MyI2C_Stop(void);
void MyI2C_SendByte(uint8_t Byte);
uint8_t MyI2C_ReceiveByte(void);
void MyI2C_SendAck(uint8_t AckBit);
uint8_t MyI2C_ReceiveAck(void);

void MyI2C_WriteCommand(uint8_t Command);
void MyI2C_WriteData(uint8_t Data);

#endif
