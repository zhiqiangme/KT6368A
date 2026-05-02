#include "MyI2C.h"

/* ���Ų����궨�� */
#define MyI2C_W_SCL(x) HAL_GPIO_WritePin(MyI2C_GPIO_PORT, MyI2C_SCL_PIN, (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)
#define MyI2C_W_SDA(x) HAL_GPIO_WritePin(MyI2C_GPIO_PORT, MyI2C_SDA_PIN, (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)
#define MyI2C_R_SDA() (HAL_GPIO_ReadPin(MyI2C_GPIO_PORT, MyI2C_SDA_PIN) == GPIO_PIN_SET ? 1 : 0)

/* ��ȷ��ʱ���� */
static void MyI2C_Delay(void)
{
    for(volatile int i = 0; i < 20; i++);  // ~5us at 72MHz, safe for 100kHz I2C
}

/* ���ų�ʼ�� */
void MyI2C_Init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();  // STM32F0��Ҳ������д
    
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = MyI2C_SCL_PIN | MyI2C_SDA_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;    // ��©���
    GPIO_InitStruct.Pull = GPIO_PULLUP;            // �ؼ�������������
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(MyI2C_GPIO_PORT, &GPIO_InitStruct);
    
    MyI2C_W_SCL(1);
    MyI2C_W_SDA(1);
}

/**
  * @brief  I2C��ʼ�ź�
  */
void MyI2C_Start(void)
{
    MyI2C_W_SDA(1);
    MyI2C_W_SCL(1);
    MyI2C_Delay();
    MyI2C_W_SDA(0);     // START: SDA�½���
    MyI2C_Delay();
    MyI2C_W_SCL(0);     // Ϊ���ݴ���׼��
}

/**
  * @brief  I2Cֹͣ�ź�
  */
void MyI2C_Stop(void)
{
    MyI2C_W_SDA(0);
    MyI2C_W_SCL(1);
    MyI2C_Delay();
    MyI2C_W_SDA(1);     // STOP: SDA������
    MyI2C_Delay();
}

/**
  * @brief  I2C����һ���ֽ�
  */
void MyI2C_SendByte(uint8_t Byte)
{
    uint8_t i;
    for (i = 0; i < 8; i++)
    {
        MyI2C_W_SDA((Byte & 0x80) >> 7);  // �������λ
        MyI2C_Delay();
        MyI2C_W_SCL(1);                   // ʱ�Ӹߵ�ƽ
        MyI2C_Delay();
        MyI2C_W_SCL(0);                   // ʱ�ӵ͵�ƽ
        MyI2C_Delay();
        Byte <<= 1;                       // ׼����һλ
    }
}

/**
  * @brief  I2C����һ���ֽ�
  */
uint8_t MyI2C_ReceiveByte(void)
{
    uint8_t i, Byte = 0;
    
    MyI2C_W_SDA(1);  // �ͷ�SDA���ôӻ�����
    
    for (i = 0; i < 8; i++)
    {
        MyI2C_W_SCL(1);           // �ͷ�SCL
        MyI2C_Delay();
        Byte <<= 1;               // ����
        if (MyI2C_R_SDA())        // ��ȡSDA
            Byte |= 0x01;
        MyI2C_W_SCL(0);           // ����SCL
        MyI2C_Delay();
    }
    return Byte;
}

/**
  * @brief  I2C����Ӧ��λ
  * @param  AckBit: 0-Ӧ��(ACK), 1-��Ӧ��(NACK)
  */
void MyI2C_SendAck(uint8_t AckBit)
{
    MyI2C_W_SDA(AckBit);        // ����ACK/NACK
    MyI2C_Delay();
    MyI2C_W_SCL(1);             // ʱ�Ӹߵ�ƽ
    MyI2C_Delay();
    MyI2C_W_SCL(0);             // ʱ�ӵ͵�ƽ
    MyI2C_Delay();
}

/**
  * @brief  I2C����Ӧ��λ
  * @retval 0-Ӧ��(ACK), 1-��Ӧ��(NACK)
  */
uint8_t MyI2C_ReceiveAck(void)
{
    uint8_t AckBit;
    uint32_t timeout = 1000;

    MyI2C_W_SDA(1);             // �ͷ�SDA���ôӻ�����
    MyI2C_Delay();
    MyI2C_W_SCL(1);             // ʱ�Ӹߵ�ƽ
    MyI2C_Delay();

    while (MyI2C_R_SDA() && timeout--);  // ���ȴӻ�Ӧ��

    AckBit = MyI2C_R_SDA();     // ��ȡACKλ
    MyI2C_W_SCL(0);             // ʱ�ӵ͵�ƽ
    MyI2C_Delay();

    return AckBit;              // 0=ACK, 1=NACK
}

/**
  * @brief  MyI2Cд�������OLED���豸��
  */
void MyI2C_WriteCommand(uint8_t Command)
{
    MyI2C_Start();
    MyI2C_SendByte(0x78);       // �ӻ���ַ
    MyI2C_ReceiveAck();         // ����ACK
    MyI2C_SendByte(0x00);       // д����
    MyI2C_ReceiveAck();         // ����ACK
    MyI2C_SendByte(Command); 
    MyI2C_ReceiveAck();         // ����ACK
    MyI2C_Stop();
}

/**
  * @brief  MyI2Cд���ݣ�����OLED���豸��
  */
void MyI2C_WriteData(uint8_t Data)
{
    MyI2C_Start();
    MyI2C_SendByte(0x78);       // �ӻ���ַ
    MyI2C_ReceiveAck();         // ����ACK
    MyI2C_SendByte(0x40);       // д����
    MyI2C_ReceiveAck();         // ����ACK
    MyI2C_SendByte(Data);
    MyI2C_ReceiveAck();         // ����ACK
    MyI2C_Stop();
}
