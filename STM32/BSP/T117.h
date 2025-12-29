#ifndef __T117_H
#define __T117_H

#include "stm32f1xx_hal.h"  // 添加这个
#include "MyI2C.h"
// #include "main.h"  // 可以去掉，除非main.h中有必要的定义

/**
 * @brief  T117传感器命令枚举
 */
typedef enum
{
    /* Temp_Cmd - 测温模式 */
    CONTI_CONVERT  = 0x00,  /* 连续测量温度 */
    STOP_CONVERT   = 0x40,  /* 停止测量温度 */
    SINGLE_CONVERT = 0xc0,  /* 单次测量温度 */
    
    /* 加热模式 */
    OFF_HEATING    = 0xf0,  /* 低位清0，加热关闭 */
    ON_HEATING     = 0x0A,  /* 加热开启 */

    /* Temp_Cfg - 测温频率 */
    FRE_8times     = 0x00,  /* 每秒8次 */
    FRE_4times     = 0x20,  /* 每秒4次 */
    FRE_2times     = 0x40,  /* 每秒2次 */
    FRE_1times     = 0x60,  /* 每秒1次 */

    FRE_2s         = 0x80,  /* 每2秒1次 */
    FRE_4s         = 0xa0,  /* 每4秒1次 */
    FRE_8s         = 0xc0,  /* 每8秒1次 */
    FRE_16s        = 0xe0,  /* 每16秒1次 */
    
    /* 平均次数 */
    AVG_1          = 0xe7,  /* 位清0，转换时间2.1ms */
    AVG_8          = 0x08,  /* 转换时间5.2ms */
    AVG_16         = 0x10,  /* 转换时间8.5ms */
    AVG_32         = 0x18,  /* 转换时间15.3ms */
    
    /* 低功耗模式 */
    OFF_PD         = 0xfe,  /* 位清0，不进入低功耗模式 */
    ON_PD          = 0x01,  /* 进入低功耗模式 */

    /* EE_Cmd */
    EE_DOWN        = 0xb6,  /* 装载EE值到寄存器 */
    EE_COPY        = 0x08,  /* 将寄存器中数值保存到EE中 */
    EE_RESET       = 0x6a,  /* 软复位,装载EE值到寄存器,与EE对应的部分，寄存器值恢复到EE保存值，不与EE对应的部分，寄存器值恢复到默认值 */

    /* Alert_Mode - 报警开关 */
    OFF_ALERT      = 0x00,  /* 清0，报警关 */
    ON_ALERT       = 0x80,  /* 报警开 */
    
    /* Mode */
    TL_CLEAR       = 0xbf,  /* 位清0，TL为报警清除门限阈值 */
    TL_ALERT       = 0x40,  /* TL为报警门限下阈值 */
    
    /* 极性 */
    ALERT_LO       = 0xdf,  /* 位清0，低电平有效 */
    ALERT_HI       = 0x20,  /* 高电平有效 */
    
    /* 报警端口模式选择 */
    ALERT_IO       = 0xef,  /* 位清0，用作温度报警 */
    CONVERT_FINI   = 0x10,  /* 用作测温完成标志 */
} I2C_CMD;

/**
 * @brief  T117寄存器地址枚举
 */
typedef enum
{
    Temp_lsb       = 0x00,  /* 温度低字节 */
    Temp_msb       = 0x01,  /* 温度高字节 */
    Crc_temp       = 0x02,  /* 温度CRC校验 */
    Status         = 0x03,  /* 状态寄存器 */
    Temp_Cmd       = 0x04,  /* 默认值0x40：停止测量，不加热 */
    Temp_Cfg       = 0x05,  /* 默认值0x69：每秒1次，AVG_8,进入低功耗 */
    Alert_Mode     = 0x06,  /* 默认值0x00：报警关，报警模式为TL解除报警，报警低电平有效，标志位表示温度报警 */
    Th_lsb         = 0x07,  /* 高温阈值低字节 */
    Th_msb         = 0x08,  /* 高温阈值高字节 */
    Tl_lsb         = 0x09,  /* 低温阈值低字节 */
    Tl_msb         = 0x0A,  /* 低温阈值高字节 */
    Crc_scratch    = 0x0B,  /* CRC校验 */
    EE_Cmd         = 0x17,  /* 默认值0x00：无操作 */
    Romcode1       = 0x18,  /* ROM代码1 */
    Romcode2       = 0x19,  /* ROM代码2 */
    Romcode3       = 0x1A,  /* ROM代码3 */
    Romcode4       = 0x1B,  /* ROM代码4 */
    Romcode5       = 0x1C,  /* ROM代码5 */
    Romcode6       = 0x1D,  /* ROM代码6 */
    Romcode7       = 0x1E,  /* ROM代码7 */
    crc_romcode    = 0x1F,  /* ROM代码CRC校验 */
} REG;

/**
 * @brief  初始化T117温度传感器
 * @param  无
 * @retval 无
 */
void T117_Init(void);

/**
 * @brief  获取传感器ID
 * @param  无
 * @retval ID值
 */
uint8_t T117_ID(void);

/**
 * @brief  读取寄存器值
 * @param  REG: 寄存器地址
 * @param  DAT: 数据存储指针
 * @retval 0:成功 其他:错误码
 */
uint8_t T117_R_REG(uint8_t REG, uint8_t *DAT);

/**
 * @brief  写入寄存器值
 * @param  REG: 寄存器地址
 * @param  DAT: 要写入的数据
 * @retval 0:成功 其他:错误码
 */
uint8_t T117_W_REG(uint8_t REG, uint8_t DAT);

/**
 * @brief  读取温度值
 * @param  DAT: 温度数据存储指针
 * @retval 0:成功 其他:错误码
 */
uint8_t T117_R_TEMP(float *DAT);

#endif
