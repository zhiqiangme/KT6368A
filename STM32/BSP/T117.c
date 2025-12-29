#include "T117.h"

/* 设备地址定义 */
#define T117_WADD 0x80  /* 写地址 */
#define T117_RADD 0x81  /* 读地址 */

/* 
 * 寄存器说明：
 * -----------------------------------------------------------------------------
 * 测温指令寄存器（Temp_Cmd），地址 0x04
 * 配置寄存器（Temp_Cfg），地址 0x05
 * 状态寄存器（Status），地址 0x03
 * -----------------------------------------------------------------------------
 */

/* 状态寄存器（Status）位定义:
 * 位 | 内容描述                | 默认数值
 * ---+------------------------+--------
 * 7  | 温度高线报警跟踪        | 0：温度报警未触发 1：温度报警触发 '0'
 * 6  | 温度低线报警跟踪        | 0：温度报警未触发 1：温度报警触发 '0'
 * 5  | 温度转换状态           | 0：温度转换完成 1：温度转换过程中 '0'
 * 4  | E2PROM 状态           | 0：未处于读写状态 1：处于读写状态 '0'
 * 3  | 加热状态              | 0：未处于加热状态 1：处于加热状态 '0'
 * 2  | 温度报警错误提示       | 0：TH 大于 TL 1：TH 小于等于 TL '0'
 * 1:0| 预留                  | '00'
 */

/* 报警模式寄存器（Alert_Mode），地址 0x06 */

/* 报警模式寄存器（Alert_Mode）位定义:
 * 位 | 内容描述                | 默认数值
 * ---+------------------------+--------
 * 7  | 报警功能开关(Alert_en)  | 0：关闭 1：开启 '0'
 * 6  | 报警模式(IM)选择        | 0：高于TH报警+低于TL解除报警 1：高于TH报警+低于TL报警 '0'
 * 5  | 报警极性(POL)选择       | 0：低有效 1：高有效 '0'
 * 4  | 报警端口输出模式(FUNC)  | 0：输出温度报警标志位 1：输出测温完成标志位 '0'
 * 3:0| 预留                   | '0000'
 */


/**
 * @brief  初始化T117温度传感器
 * @param  无
 * @retval 无
 */
void T117_Init(void)
{
    MyI2C_Init();

    /* 设置测温指令寄存器：连续测温模式，不启动加热功能 */
    T117_W_REG(Temp_Cmd, 0x00);  /* 连续测温不加热 */
    
    /* 设置配置寄存器：
     * 0x99 = 1001 1001b
     * 位7-5: 001 - 每秒0.5次采样频率（对应FRE_2times的一半）
     * 位4-3: 10 - 32次平均采样（对应AVG_32）
     * 位0:   1 - 进入低功耗模式（对应ON_PD）
     */
    T117_W_REG(Temp_Cfg, 0x99);  /* 每秒0.5次，32次平均，进入低功耗 */
    
    /* 设置报警模式寄存器：
     * 0x10 = 0001 0000b
     * 位7: 0 - 报警功能关闭
     * 位4: 1 - 报警端口用作测温完成标志位（对应CONVERT_FINI）
     */
    T117_W_REG(Alert_Mode, 0x10); /* 不报警，输出测温完成标志位 */
}

/**
 * @brief  获取传感器ID
 * @param  无
 * @retval ID值：成功返回传感器ID，失败返回错误码(1-3)
 */
uint8_t T117_ID(void)
{
    uint8_t temp = 0;
    
    /* 第1步：发送I2C起始信号 */
    MyI2C_Start();
    
    /* 第2步：发送设备写地址(0x80)，并等待应答 */
    MyI2C_SendByte(T117_WADD);
    temp = MyI2C_ReceiveAck();
    /* 如果无应答，则发送停止信号并返回错误码1 */
    if(temp) {MyI2C_Stop(); return 1;}
    
    /* 第3步：发送ROM代码2寄存器地址(0x19)，并等待应答 */
    MyI2C_SendByte(0x19);  /* Romcode2寄存器，存储设备ID */
    temp = MyI2C_ReceiveAck();
    /* 如果无应答，则发送停止信号并返回错误码2 */
    if(temp) {MyI2C_Stop(); return 2;}

    /* 第4步：发送重复起始信号，准备读取数据 */
    MyI2C_Start();
    
    /* 第5步：发送设备读地址(0x81)，并等待应答 */
    MyI2C_SendByte(T117_RADD);
    temp = MyI2C_ReceiveAck();
    /* 如果无应答，则发送停止信号并返回错误码3 */
    if(temp) {MyI2C_Stop(); return 3;}
    
    /* 第6步：读取一个字节的ID数据 */
    temp = MyI2C_ReceiveByte();
    
    /* 第7步：发送非应答信号，表示结束读取 */
    MyI2C_SendAck(1);
    
    /* 第8步：发送停止信号，结束通信 */
    MyI2C_Stop();
    
    /* 返回读取到的ID值 */
    return temp;
}

/**
 * @brief  读取寄存器值
 * @param  REG: 寄存器地址
 * @param  DAT: 数据存储指针
 * @retval 0:成功 其他:错误码
 *         1: 发送设备地址失败
 *         2: 发送寄存器地址失败
 *         3: 读取数据时设备无应答
 */
uint8_t T117_R_REG(uint8_t REG, uint8_t *DAT)
{
    uint8_t temp = 0;
    
    /* 第1步：发送I2C起始信号，开始通信 */
    MyI2C_Start();
    
    /* 第2步：发送设备写地址(0x80)，表示要写入数据 */
    MyI2C_SendByte(T117_WADD);
    temp = MyI2C_ReceiveAck();
    /* 检查应答信号，如果无应答则终止通信并返回错误码1 */
    if(temp) {MyI2C_Stop(); return 1;}
    
    /* 第3步：发送要读取的寄存器地址 */
    MyI2C_SendByte(REG);
    temp = MyI2C_ReceiveAck();
    /* 检查应答信号，如果无应答则终止通信并返回错误码2 */
    if(temp) {MyI2C_Stop(); return 2;}

    /* 第4步：发送重复起始信号，准备读取数据 */
    MyI2C_Start();
    
    /* 第5步：发送设备读地址(0x81)，表示要读取数据 */
    MyI2C_SendByte(T117_RADD);
    temp = MyI2C_ReceiveAck();
    /* 检查应答信号，如果无应答则终止通信并返回错误码3 */
    if(temp) {MyI2C_Stop(); return 3;}
    
    /* 第6步：读取寄存器数据并存储到指针指向的变量 */
    *DAT = MyI2C_ReceiveByte();
    
    /* 第7步：发送非应答信号，表示结束读取 */
    MyI2C_SendAck(1);
    
    /* 第8步：发送停止信号，结束通信 */
    MyI2C_Stop();
    
    /* 返回成功标志 */
    return 0;
}

/**
 * @brief  写入寄存器值
 * @param  REG: 寄存器地址
 * @param  DAT: 要写入的数据
 * @retval 0:成功 其他:错误码
 *         1: 发送设备地址失败
 *         2: 发送寄存器地址失败
 *         3: 发送数据失败
 */
uint8_t T117_W_REG(uint8_t REG, uint8_t DAT)
{
    uint8_t temp = 0;
    
    /* 第1步：发送I2C起始信号，开始通信 */
    MyI2C_Start();
    
    /* 第2步：发送设备写地址(0x80) */
    MyI2C_SendByte(T117_WADD);
    temp = MyI2C_ReceiveAck();
    /* 检查应答信号，如果无应答则终止通信并返回错误码1 */
    if(temp) {MyI2C_Stop(); return 1;}

    /* 第3步：发送要写入的寄存器地址 */
    MyI2C_SendByte(REG);
    temp = MyI2C_ReceiveAck();
    /* 检查应答信号，如果无应答则终止通信并返回错误码2 */
    if(temp) {MyI2C_Stop(); return 2;}

    /* 第4步：发送要写入的数据 */
    MyI2C_SendByte(DAT);
    temp = MyI2C_ReceiveAck();
    /* 检查应答信号，如果无应答则终止通信并返回错误码3 */
    if(temp) {MyI2C_Stop(); return 3;}
    
    /* 第5步：发送停止信号，结束通信 */
    MyI2C_Stop();
    
    /* 返回成功标志 */
    return 0;
}

/**
 * @brief  读取温度值
 * @param  DAT: 温度数据存储指针
 * @retval 0:成功 其他:错误码
 *         1: 读取温度高字节失败
 *         2: 读取温度低字节失败
 */
uint8_t T117_R_TEMP(float *DAT)
{
    uint16_t rx = 0;     /* 用于存储组合后的16位温度原始数据 */
    uint8_t data = 0;    /* 临时存储单个字节的数据 */

    /* 第1步：读取温度高字节(MSB)，地址0x01 */
    if(T117_R_REG(0x01, &data)) return 1;
    rx = data;           /* 将高字节数据存入rx */
    rx <<= 8;            /* 左移8位，为低字节预留位置 */
    
    /* 第2步：读取温度低字节(LSB)，地址0x00 */
    if(T117_R_REG(0x00, &data)) return 2;
    rx += data;          /* 将低字节数据加入rx，组成完整的16位温度数据 */
    
    /* 第3步：将16位原始数据转换为有符号整数，以处理负温度 */
    *DAT = (int16_t)rx;
    
    /* 第4步：将原始数据转换为实际温度值（摄氏度）
     * 转换公式: 温度 = 25.0 + 原始数据 / 256.0
     * 说明: 25.0是基准温度，原始数据表示相对于25℃的偏移量
     * 每1个单位代表1/256℃的温度变化
     */
    *DAT = 25.0 + (*DAT) / 256.0;
    
    /* 返回成功标志 */
    return 0;
}
