#include "MyI2C.h"

/* 引脚操作宏定义 */
#define MyI2C_W_SCL(x) HAL_GPIO_WritePin(MyI2C_GPIO_PORT, MyI2C_SCL_PIN, (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)
#define MyI2C_W_SDA(x) HAL_GPIO_WritePin(MyI2C_GPIO_PORT, MyI2C_SDA_PIN, (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)
#define MyI2C_R_SDA() (HAL_GPIO_ReadPin(MyI2C_GPIO_PORT, MyI2C_SDA_PIN) == GPIO_PIN_SET ? 1 : 0)

/* 精确延时函数 */
static void MyI2C_Delay(void)
{
    for(volatile int i = 0; i < 20; i++);  // 约5us@72MHz，满足100kHz I2C时序要求
}

/* 引脚初始化 */
void MyI2C_Init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();  // 使能GPIOB时钟

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = MyI2C_SCL_PIN | MyI2C_SDA_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;    // 开漏输出
    GPIO_InitStruct.Pull = GPIO_PULLUP;            // 内部上拉
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(MyI2C_GPIO_PORT, &GPIO_InitStruct);

    MyI2C_W_SCL(1);
    MyI2C_W_SDA(1);
}

/**
  * @brief  I2C起始信号
  */
void MyI2C_Start(void)
{
    MyI2C_W_SDA(1);
    MyI2C_W_SCL(1);
    MyI2C_Delay();
    MyI2C_W_SDA(0);     // START: SDA下降沿
    MyI2C_Delay();
    MyI2C_W_SCL(0);     // 为数据传输准备
}

/**
  * @brief  I2C停止信号
  */
void MyI2C_Stop(void)
{
    MyI2C_W_SDA(0);
    MyI2C_W_SCL(1);
    MyI2C_Delay();
    MyI2C_W_SDA(1);     // STOP: SDA上升沿
    MyI2C_Delay();
}

/**
  * @brief  I2C发送一个字节
  */
void MyI2C_SendByte(uint8_t Byte)
{
    uint8_t i;
    for (i = 0; i < 8; i++)
    {
        MyI2C_W_SDA((Byte & 0x80) >> 7);  // 发送最高位
        MyI2C_Delay();
        MyI2C_W_SCL(1);                   // 时钟高电平
        MyI2C_Delay();
        MyI2C_W_SCL(0);                   // 时钟低电平
        MyI2C_Delay();
        Byte <<= 1;                       // 准备下一位
    }
}

/**
  * @brief  I2C接收一个字节
  */
uint8_t MyI2C_ReceiveByte(void)
{
    uint8_t i, Byte = 0;

    MyI2C_W_SDA(1);  // 释放SDA，让从机控制

    for (i = 0; i < 8; i++)
    {
        MyI2C_W_SCL(1);           // 释放SCL
        MyI2C_Delay();
        Byte <<= 1;               // 移位
        if (MyI2C_R_SDA())        // 读取SDA
            Byte |= 0x01;
        MyI2C_W_SCL(0);           // 拉低SCL
        MyI2C_Delay();
    }
    return Byte;
}

/**
  * @brief  I2C发送应答位
  * @param  AckBit: 0-应答(ACK), 1-非应答(NACK)
  */
void MyI2C_SendAck(uint8_t AckBit)
{
    MyI2C_W_SDA(AckBit);        // 发送ACK/NACK
    MyI2C_Delay();
    MyI2C_W_SCL(1);             // 时钟高电平
    MyI2C_Delay();
    MyI2C_W_SCL(0);             // 时钟低电平
    MyI2C_Delay();
}

/**
  * @brief  I2C接收应答位
  * @retval 0-应答(ACK), 1-非应答(NACK)
  */
uint8_t MyI2C_ReceiveAck(void)
{
    uint8_t AckBit;
    uint32_t timeout = 1000;

    MyI2C_W_SDA(1);             // 释放SDA，让从机控制
    MyI2C_Delay();
    MyI2C_W_SCL(1);             // 时钟高电平
    MyI2C_Delay();

    while (MyI2C_R_SDA() && timeout--);  // 等待从机应答

    AckBit = MyI2C_R_SDA();     // 读取ACK位
    MyI2C_W_SCL(0);             // 时钟低电平
    MyI2C_Delay();

    return AckBit;              // 0=ACK, 1=NACK
}

/**
  * @brief  MyI2C写命令（适用于OLED等设备）
  */
void MyI2C_WriteCommand(uint8_t Command)
{
    MyI2C_Start();
    MyI2C_SendByte(0x78);       // 从机地址
    MyI2C_ReceiveAck();         // 等待ACK
    MyI2C_SendByte(0x00);       // 写命令
    MyI2C_ReceiveAck();         // 等待ACK
    MyI2C_SendByte(Command);
    MyI2C_ReceiveAck();         // 等待ACK
    MyI2C_Stop();
}

/**
  * @brief  MyI2C写数据（适用于OLED等设备）
  */
void MyI2C_WriteData(uint8_t Data)
{
    MyI2C_Start();
    MyI2C_SendByte(0x78);       // 从机地址
    MyI2C_ReceiveAck();         // 等待ACK
    MyI2C_SendByte(0x40);       // 写数据
    MyI2C_ReceiveAck();         // 等待ACK
    MyI2C_SendByte(Data);
    MyI2C_ReceiveAck();         // 等待ACK
    MyI2C_Stop();
}
