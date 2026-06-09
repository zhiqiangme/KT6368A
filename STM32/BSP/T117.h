#ifndef __T117_H
#define __T117_H

#include "stm32f1xx_hal.h"
#include "MyI2C.h"

/**
 * @brief  T117命令参数枚举
 */
typedef enum
{
    /* Temp_Cmd - 转换模式 */
    CONTI_CONVERT  = 0x00,  /* 连续转换温度 */
    STOP_CONVERT   = 0x40,  /* 停止转换温度 */
    SINGLE_CONVERT = 0xc0,  /* 单次转换温度 */

    /* 加热模式 */
    OFF_HEATING    = 0xf0,  /* 复位并关闭加热 */
    ON_HEATING     = 0x0A,  /* 开启加热 */

    /* Temp_Cfg - 采样频率 */
    FRE_8times     = 0x00,  /* 每秒8次 */
    FRE_4times     = 0x20,  /* 每秒4次 */
    FRE_2times     = 0x40,  /* 每秒2次 */
    FRE_1times     = 0x60,  /* 每秒1次 */

    FRE_2s         = 0x80,  /* 每2秒1次 */
    FRE_4s         = 0xa0,  /* 每4秒1次 */
    FRE_8s         = 0xc0,  /* 每8秒1次 */
    FRE_16s        = 0xe0,  /* 每16秒1次 */

    /* 平均次数 */
    AVG_1          = 0xe7,  /* 1次（转换时间2.1ms） */
    AVG_8          = 0x08,  /* 8次（转换时间5.2ms） */
    AVG_16         = 0x10,  /* 16次（转换时间8.5ms） */
    AVG_32         = 0x18,  /* 32次（转换时间15.3ms） */

    /* 低功耗模式 */
    OFF_PD         = 0xfe,  /* 关闭低功耗模式 */
    ON_PD          = 0x01,  /* 开启低功耗模式 */

    /* EE_Cmd */
    EE_DOWN        = 0xb6,  /* 装载EE值到寄存器 */
    EE_COPY        = 0x08,  /* 将寄存器当前值保存到EE */
    EE_RESET       = 0x6a,  /* 复位，装载EE值到寄存器，EE对应的部分，寄存器值恢复为默认值 */

    /* Alert_Mode - 报警配置 */
    OFF_ALERT      = 0x00,  /* 关闭报警功能 */
    ON_ALERT       = 0x80,  /* 开启报警 */

    /* Mode */
    TL_CLEAR       = 0xbf,  /* TL为迟滞回滞清除值 */
    TL_ALERT       = 0x40,  /* TL为报警触发值 */

    /* 极性 */
    ALERT_LO       = 0xdf,  /* 低电平有效 */
    ALERT_HI       = 0x20,  /* 高电平有效 */

    /* 报警引脚模式选择 */
    ALERT_IO       = 0xef,  /* 报警输出温度报警标志 */
    CONVERT_FINI   = 0x10,  /* 报警输出转换完成标志 */
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
    Temp_Cmd       = 0x04,  /* 默认值0x40，停止转换 */
    Temp_Cfg       = 0x05,  /* 默认值0x69，每秒1次，AVG_8，关闭低功耗 */
    Alert_Mode     = 0x06,  /* 默认值0x00，报警关闭，迟滞模式为TL迟滞，低电平有效，标志位表示温度报警 */
    Th_lsb         = 0x07,  /* 阈值高字节低字节 */
    Th_msb         = 0x08,  /* 阈值高字节高字节 */
    Tl_lsb         = 0x09,  /* 阈值低字节低字节 */
    Tl_msb         = 0x0A,  /* 阈值低字节高字节 */
    Crc_scratch    = 0x0B,  /* CRC校验 */
    EE_Cmd         = 0x17,  /* 默认值0x00，无操作 */
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
 * @retval 0:成功 其他:错误代码
 */
uint8_t T117_Init(void);

/**
 * @brief  读取设备ID
 * @retval ID值
 */
uint8_t T117_ID(void);

/**
 * @brief  读取寄存器值
 * @param  REG: 寄存器地址
 * @param  DAT: 数据存储指针
 * @retval 0:成功 其他:错误代码
 */
uint8_t T117_R_REG(uint8_t REG, uint8_t *DAT);

/**
 * @brief  写寄存器值
 * @param  REG: 寄存器地址
 * @param  DAT: 要写入的数据
 * @retval 0:成功 其他:错误代码
 */
uint8_t T117_W_REG(uint8_t REG, uint8_t DAT);

/**
 * @brief  读取温度值
 * @param  DAT: 温度数据存储指针
 * @retval 0:成功 其他:错误代码
 */
uint8_t T117_R_TEMP(float *DAT);

#endif
