/*
 * t117_sensor.h
 *
 *  T117数字温度传感器头文件，基于STM32 HAL库实现
 *  使用I2C接口
 *  连接方式:
 *    - ALERT -> PA4 (配置为输入)
 *    - SDA   -> PB11 (I2C1 SDA)
 *    - SCL   -> PB10 (I2C1 SCL)
 *    - ADDR  -> GND (假设值 - 请确认!)
 */

#ifndef INC_T117_SENSOR_H_
#define INC_T117_SENSOR_H_

#include "stm32f0xx_hal.h" // 根据您特定的STM32F0系列调整包含文件（如需要）
#include <stdint.h>

// --- 配置 ---
// 请根据ADDR引脚连接确认I2C地址
// ADDR -> GND: 0x40, VDD: 0x41, SDA: 0x42, SCL: 0x43
#define T117_I2C_ADDRESS    (0x40 << 1) // 7位地址左移1位以适应HAL（ADDR=GND时为0x80）

// --- T117寄存器地址 ---
#define T117_REG_TEMP_LSB   0x00
#define T117_REG_TEMP_MSB   0x01
// 温度寄存器的CRC (0x00, 0x01) 位于 0x02
#define T117_REG_STATUS     0x03
#define T117_REG_TEMP_CMD   0x04
#define T117_REG_TEMP_CFG   0x05
#define T117_REG_ALERT_MODE 0x06
#define T117_REG_TH_LSB     0x07
#define T117_REG_TH_MSB     0x08
#define T117_REG_TL_LSB     0x09
#define T117_REG_TL_MSB     0x0A
// 暂存器寄存器的CRC (0x03 到 0x0A) 位于 0x0B
#define T117_REG_USER_0     0x0C
// ... 其他用户寄存器直到 0x15
// 暂存器扩展的CRC (0x0C 到 0x15) 位于 0x16
#define T117_REG_EEPROM_CMD 0x17
#define T117_REG_DEVICE_ID_LSB 0x18
#define T117_REG_DEVICE_ID_MSB 0x19

// --- T117命令值 ---
#define T117_CMD_CONVERT_SINGLE     0x90 // 位7:6 = 11 (单次), 位3:0 = 0000 (无加热)
#define T117_CMD_CONVERT_CONTINUOUS 0x00 // 位7:6 = 00 (连续), 位3:0 = 0000 (无加热)
#define T117_CMD_STOP               0x40 // 位7:6 = 01 (停止), 位3:0 = 0000 (无加热)
#define T117_CMD_HEATER_ON          0x9A // 位7:6 = 10 (连续虚拟), 位3:0 = 1010 (加热开)
#define T117_CMD_HEATER_OFF         0x90 // 位7:6 = 10 (连续虚拟), 位3:0 = 0000 (加热关)

#define T117_EEPROM_CMD_RECALL      0xB6
#define T117_EEPROM_CMD_COPY        0x08
#define T117_EEPROM_CMD_RESET       0x6A

// --- T117配置寄存器位定义 ---
// MPS (每秒测量次数) - 位7:5
#define T117_CFG_MPS_8_HZ           (0x00 << 5)
#define T117_CFG_MPS_4_HZ           (0x01 << 5)
#define T117_CFG_MPS_2_HZ           (0x02 << 5)
#define T117_CFG_MPS_1_HZ           (0x03 << 5)
#define T117_CFG_MPS_0_5_HZ         (0x04 << 5)
#define T117_CFG_MPS_0_25_HZ        (0x05 << 5)
#define T117_CFG_MPS_0_125_HZ       (0x06 << 5)
#define T117_CFG_MPS_0_0625_HZ      (0x07 << 5)

// AVG (平均值) - 位4:3
#define T117_CFG_AVG_1              (0x00 << 3)
#define T117_CFG_AVG_8              (0x01 << 3)
#define T117_CFG_AVG_16             (0x02 << 3)
#define T117_CFG_AVG_32             (0x03 << 3)

// 睡眠使能 - 位0
#define T117_CFG_SLEEP_ENABLE       (0x01 << 0)
#define T117_CFG_SLEEP_DISABLE      (0x00 << 0)

// --- T117状态寄存器位定义 ---
#define T117_STATUS_BIT_ALERT_HIGH  7
#define T117_STATUS_BIT_ALERT_LOW   6
#define T117_STATUS_BIT_BUSY        5
#define T117_STATUS_BIT_EEPROM_BUSY 4
#define T117_STATUS_BIT_HEATER_ON   3


// --- 函数返回码 ---
typedef enum {
    T117_OK = 0,
    T117_ERROR = 1,
    T117_BUSY = 2,
    T117_TIMEOUT = 3
} T117_StatusTypeDef;


// --- 函数原型 ---

/**
 * @brief  初始化T117传感器GPIO(ALERT引脚)
 *         假设I2C外设已由HAL/MCube初始化
 * @param  无
 * @retval 无
 */
void T117_Init_GPIO(void);

/**
 * @brief  从传感器读取原始16位温度值
 * @param  hi2c I2C句柄指针(例如 &hi2c1)
 * @param  raw_temp 存储原始温度数据的指针
 * @retval T117_StatusTypeDef 成功返回T117_OK, I2C失败返回T117_ERROR
 */
T117_StatusTypeDef T117_Read_Raw_Temperature(I2C_HandleTypeDef *hi2c, int16_t *raw_temp);

/**
 * @brief  将原始16位温度值转换为摄氏度
 * @param  raw_temp 来自传感器的16位有符号原始温度值
 * @return 摄氏度的温度值(float类型)
 */
float T117_Convert_Raw_To_Celsius(int16_t raw_temp);

/**
 * @brief  在传感器上启动单次温度转换
 * @param  hi2c I2C句柄指针(例如 &hi2c1)
 * @retval T117_StatusTypeDef 成功返回T117_OK, I2C失败返回T117_ERROR
 */
T117_StatusTypeDef T117_Start_Single_Conversion(I2C_HandleTypeDef *hi2c);

/**
 * @brief  检查传感器是否正在执行转换
 * @param  hi2c I2C句柄指针(例如 &hi2c1)
 * @param  is_busy 存储忙状态的指针(1表示忙,0表示空闲)
 * @retval T117_StatusTypeDef 成功返回T117_OK, I2C失败返回T117_ERROR
 */
T117_StatusTypeDef T117_Is_Busy(I2C_HandleTypeDef *hi2c, uint8_t *is_busy);

/**
 * @brief  读取传感器的状态寄存器
 * @param  hi2c I2C句柄指针(例如 &hi2c1)
 * @param  status 存储8位状态寄存器值的指针
 * @retval T117_StatusTypeDef 成功返回T117_OK, I2C失败返回T117_ERROR
 */
T117_StatusTypeDef T117_Read_Status(I2C_HandleTypeDef *hi2c, uint8_t *status);

/**
 * @brief  读取传感器的设备ID
 * @param  hi2c I2C句柄指针(例如 &hi2c1)
 * @param  device_id 存储16位设备ID的指针
 * @retval T117_StatusTypeDef 成功返回T117_OK, I2C失败返回T117_ERROR
 */
T117_StatusTypeDef T117_Read_Device_ID(I2C_HandleTypeDef *hi2c, uint16_t *device_id);

/**
 * @brief  读取ALERT引脚状态
 * @param  alert_state 存储状态的指针(GPIO_PIN_SET或GPIO_PIN_RESET)
 * @retval 无
 */
void T117_Read_Alert_Pin(GPIO_PinState *alert_state);

/**
 * @brief  对传感器执行软复位
 * @param  hi2c I2C句柄指针(例如 &hi2c1)
 * @retval T117_StatusTypeDef 成功返回T117_OK, I2C失败返回T117_ERROR
 */
T117_StatusTypeDef T117_Soft_Reset(I2C_HandleTypeDef *hi2c);

/**
 * @brief  配置传感器的测量设置
 * @param  hi2c I2C句柄指针(例如 &hi2c1)
 * @param  config_byte 要写入Temp_Cfg寄存器(0x05)的配置字节
 * @retval T117_StatusTypeDef 成功返回T117_OK, I2C失败返回T117_ERROR
 */
T117_StatusTypeDef T117_Write_Config(I2C_HandleTypeDef *hi2c, uint8_t config_byte);

/**
 * @brief  读取传感器的配置寄存器
 * @param  hi2c I2C句柄指针(例如 &hi2c1)
 * @param  config_byte 存储8位配置寄存器值的指针
 * @retval T117_StatusTypeDef 成功返回T117_OK, I2C失败返回T117_ERROR
 */
T117_StatusTypeDef T117_Read_Config(I2C_HandleTypeDef *hi2c, uint8_t *config_byte);

#endif /* INC_T117_SENSOR_H_ */