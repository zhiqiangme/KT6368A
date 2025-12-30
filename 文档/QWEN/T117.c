/*
 * t117_sensor.c
 *
 *  T117数字温度传感器源文件，基于STM32 HAL库实现
 *  使用I2C接口
 *  连接方式:
 *    - ALERT -> PA4 (配置为输入)
 *    - SDA   -> PB11 (I2C1 SDA)
 *    - SCL   -> PB10 (I2C1 SCL)
 *    - ADDR  -> GND (假设值 - 请确认!)
 */

#include "t117_sensor.h"
#include <string.h> // 用于memset

// 定义I2C超时时间(根据需要调整)
#define T117_I2C_TIMEOUT_MS 100

// 定义ALERT引脚的GPIO端口和引脚
#define T117_ALERT_GPIO_PORT GPIOA
#define T117_ALERT_PIN       GPIO_PIN_4


/**
 * @brief  初始化T117传感器GPIO (ALERT引脚)。
 *         假设I2C外设已由HAL/MCube初始化。
 * @param  无
 * @retval 无
 */
void T117_Init_GPIO(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // 使能ALERT引脚的GPIO时钟
    __HAL_RCC_GPIOA_CLK_ENABLE(); // 假设ALERT在GPIOA上

    // 配置ALERT引脚为输入
    GPIO_InitStruct.Pin = T117_ALERT_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL; // 传感器有内部上拉/下拉可配置吗？规格书显示开漏。假设外部或内部拉取已处理（如需要）。
    HAL_GPIO_Init(T117_ALERT_GPIO_PORT, &GPIO_InitStruct);

    // 注意: 如果需要使用ALERT作为中断，请在此配置EXTI
}

/**
 * @brief  从传感器读取原始16位温度值。
 * @param  hi2c 指向I2C句柄的指针 (例如, &hi2c1)
 * @param  raw_temp 指向存储16位原始温度数据的指针。
 * @retval T117_StatusTypeDef 成功时返回T117_OK, I2C失败时返回T117_ERROR。
 */
T117_StatusTypeDef T117_Read_Raw_Temperature(I2C_HandleTypeDef *hi2c, int16_t *raw_temp) {
    if (hi2c == NULL || raw_temp == NULL) {
        return T117_ERROR;
    }

    uint8_t temp_buffer[2] = {0};
    HAL_StatusTypeDef hal_status;

    // 从TEMP_LSB寄存器(0x00)开始读取2字节
    // 传感器会自动增加多字节读取的地址
    hal_status = HAL_I2C_Mem_Read(hi2c, T117_I2C_ADDRESS, T117_REG_TEMP_LSB, I2C_MEMADD_SIZE_8BIT, temp_buffer, 2, T117_I2C_TIMEOUT_MS);

    if (hal_status != HAL_OK) {
        // 如果需要，检查具体错误(例如 HAL_I2C_GetError(hi2c))
        return T117_ERROR;
    }

    // 将LSB和MSB组合成16位有符号整数
    // T117使用标准字节序(LSB在前)
    *raw_temp = (int16_t)((temp_buffer[1] << 8) | temp_buffer[0]);

    return T117_OK;
}

/**
 * @brief  将原始16位温度值转换为摄氏度。
 *         数据手册公式: Temp = RawTemp / 256.0
 * @param  raw_temp 来自传感器的16位有符号原始温度值。
 * @return 以浮点数表示的摄氏温度。
 */
float T117_Convert_Raw_To_Celsius(int16_t raw_temp) {
    // 分辨率为每LSB 1/256°C
    return (float)raw_temp / 256.0f;
}

/**
 * @brief  在传感器上启动单次温度转换。
 * @param  hi2c 指向I2C句柄的指针 (例如, &hi2c1)
 * @retval T117_StatusTypeDef 成功时返回T117_OK, I2C失败时返回T117_ERROR。
 */
T117_StatusTypeDef T117_Start_Single_Conversion(I2C_HandleTypeDef *hi2c) {
    if (hi2c == NULL) {
        return T117_ERROR;
    }

    uint8_t cmd = T117_CMD_CONVERT_SINGLE; // 0x90: 单次拍摄, 无加热
    HAL_StatusTypeDef hal_status;

    // 将命令写入Temp_Cmd寄存器(0x04)
    hal_status = HAL_I2C_Mem_Write(hi2c, T117_I2C_ADDRESS, T117_REG_TEMP_CMD, I2C_MEMADD_SIZE_8BIT, &cmd, 1, T117_I2C_TIMEOUT_MS);

    if (hal_status != HAL_OK) {
        return T117_ERROR;
    }

    return T117_OK;
}

/**
 * @brief  检查传感器是否正在执行转换。
 * @param  hi2c 指向I2C句柄的指针 (例如, &hi2c1)
 * @param  is_busy 指向存储忙状态的指针 (1表示忙, 0表示空闲)。
 * @retval T117_StatusTypeDef 成功时返回T117_OK, I2C失败时返回T117_ERROR。
 */
T117_StatusTypeDef T117_Is_Busy(I2C_HandleTypeDef *hi2c, uint8_t *is_busy) {
     if (hi2c == NULL || is_busy == NULL) {
        return T117_ERROR;
    }

    uint8_t status_reg = 0;
    T117_StatusTypeDef t117_status = T117_Read_Status(hi2c, &status_reg);

    if (t117_status != T117_OK) {
        return t117_status; // 传播错误
    }

    // 检查状态寄存器的位5(T117_STATUS_BIT_BUSY)
    if (status_reg & (1 << T117_STATUS_BIT_BUSY)) {
        *is_busy = 1;
    } else {
        *is_busy = 0;
    }

    return T117_OK;
}

/**
 * @brief  读取传感器的状态寄存器。
 * @param  hi2c 指向I2C句柄的指针 (例如, &hi2c1)
 * @param  status 指向存储8位状态寄存器值的指针。
 * @retval T117_StatusTypeDef 成功时返回T117_OK, I2C失败时返回T117_ERROR。
 */
T117_StatusTypeDef T117_Read_Status(I2C_HandleTypeDef *hi2c, uint8_t *status) {
    if (hi2c == NULL || status == NULL) {
        return T117_ERROR;
    }

    HAL_StatusTypeDef hal_status;

    hal_status = HAL_I2C_Mem_Read(hi2c, T117_I2C_ADDRESS, T117_REG_STATUS, I2C_MEMADD_SIZE_8BIT, status, 1, T117_I2C_TIMEOUT_MS);

    if (hal_status != HAL_OK) {
        return T117_ERROR;
    }

    return T117_OK;
}

/**
 * @brief  读取传感器的设备ID。
 * @param  hi2c 指向I2C句柄的指针 (例如, &hi2c1)
 * @param  device_id 指向存储16位设备ID的指针。
 * @retval T117_StatusTypeDef 成功时返回T117_OK, I2C失败时返回T117_ERROR。
 */
T117_StatusTypeDef T117_Read_Device_ID(I2C_HandleTypeDef *hi2c, uint16_t *device_id) {
    if (hi2c == NULL || device_id == NULL) {
        return T117_ERROR;
    }

    uint8_t id_buffer[2] = {0};
    HAL_StatusTypeDef hal_status;

    // 从DEVICE_ID_LSB寄存器(0x18)开始读取2字节
    hal_status = HAL_I2C_Mem_Read(hi2c, T117_I2C_ADDRESS, T117_REG_DEVICE_ID_LSB, I2C_MEMADD_SIZE_8BIT, id_buffer, 2, T117_I2C_TIMEOUT_MS);

    if (hal_status != HAL_OK) {
        return T117_ERROR;
    }

    // 将LSB和MSB组合成16位无符号整数
    // 假设ID也使用标准字节序(LSB在前)
    *device_id = (uint16_t)((id_buffer[1] << 8) | id_buffer[0]);

    // 根据数据手册表9.11，预期ID为0x1601
    // 如果需要，可以在此添加检查

    return T117_OK;
}

/**
 * @brief  读取ALERT引脚状态。
 * @param  alert_state 指向存储状态的指针 (GPIO_PIN_SET 或 GPIO_PIN_RESET)。
 * @retval 无
 */
void T117_Read_Alert_Pin(GPIO_PinState *alert_state) {
    if (alert_state != NULL) {
        *alert_state = HAL_GPIO_ReadPin(T117_ALERT_GPIO_PORT, T117_ALERT_PIN);
    }
}

/**
 * @brief  对传感器执行软复位。
 * @param  hi2c 指向I2C句柄的指针 (例如, &hi2c1)
 * @retval T117_StatusTypeDef 成功时返回T117_OK, I2C失败时返回T117_ERROR。
 */
T117_StatusTypeDef T117_Soft_Reset(I2C_HandleTypeDef *hi2c) {
    if (hi2c == NULL) {
        return T117_ERROR;
    }

    uint8_t cmd = T117_EEPROM_CMD_RESET; // 0x6A
    HAL_StatusTypeDef hal_status;

    // 将复位命令写入EEPROM_Cmd寄存器(0x17)
    hal_status = HAL_I2C_Mem_Write(hi2c, T117_I2C_ADDRESS, T117_REG_EEPROM_CMD, I2C_MEMADD_SIZE_8BIT, &cmd, 1, T117_I2C_TIMEOUT_MS);

    if (hal_status != HAL_OK) {
        return T117_ERROR;
    }

    // 可选: 如果需要，复位后添加小延迟
    HAL_Delay(1); // 1毫秒延迟，根据传感器启动时间调整

    return T117_OK;
}

/**
 * @brief  配置传感器的测量设置。
 * @param  hi2c 指向I2C句柄的指针 (例如, &hi2c1)
 * @param  config_byte 要写入Temp_Cfg寄存器(0x05)的配置字节。
 * @retval T117_StatusTypeDef 成功时返回T117_OK, I2C失败时返回T117_ERROR。
 */
T117_StatusTypeDef T117_Write_Config(I2C_HandleTypeDef *hi2c, uint8_t config_byte) {
    if (hi2c == NULL) {
        return T117_ERROR;
    }

    HAL_StatusTypeDef hal_status;

    // 将配置字节写入Temp_Cfg寄存器(0x05)
    hal_status = HAL_I2C_Mem_Write(hi2c, T117_I2C_ADDRESS, T117_REG_TEMP_CFG, I2C_MEMADD_SIZE_8BIT, &config_byte, 1, T117_I2C_TIMEOUT_MS);

    if (hal_status != HAL_OK) {
        return T117_ERROR;
    }

    return T117_OK;
}

/**
 * @brief  读取传感器的配置寄存器。
 * @param  hi2c 指向I2C句柄的指针 (例如, &hi2c1)
 * @param  config_byte 指向存储8位配置寄存器值的指针。
 * @retval T117_StatusTypeDef 成功时返回T117_OK, I2C失败时返回T117_ERROR。
 */
T117_StatusTypeDef T117_Read_Config(I2C_HandleTypeDef *hi2c, uint8_t *config_byte) {
     if (hi2c == NULL || config_byte == NULL) {
        return T117_ERROR;
    }

    HAL_StatusTypeDef hal_status;

    hal_status = HAL_I2C_Mem_Read(hi2c, T117_I2C_ADDRESS, T117_REG_TEMP_CFG, I2C_MEMADD_SIZE_8BIT, config_byte, 1, T117_I2C_TIMEOUT_MS);

    if (hal_status != HAL_OK) {
        return T117_ERROR;
    }

    return T117_OK;
}

// --- 示例使用辅助函数(可选) ---

/**
 * @brief  等待单次转换完成(轮询方式)
 * @param  hi2c I2C句柄指针(例如 &hi2c1)
 * @param  timeout_ms 最大等待时间(毫秒)
 * @retval T117_StatusTypeDef 转换完成返回T117_OK, 超时返回T117_TIMEOUT, I2C错误返回T117_ERROR
 */
T117_StatusTypeDef T117_Wait_For_Conversion(I2C_HandleTypeDef *hi2c, uint32_t timeout_ms) {
    uint32_t start_tick = HAL_GetTick();
    uint8_t busy = 1;
    T117_StatusTypeDef status;

    do {
        status = T117_Is_Busy(hi2c, &busy);
        if (status != T117_OK) {
            return status; // 传播I2C错误
        }

        if ((HAL_GetTick() - start_tick) >= timeout_ms) {
            return T117_TIMEOUT;
        }

        // 小延迟以避免频繁访问I2C总线
        HAL_Delay(1);

    } while (busy);

    return T117_OK;
}

/**
 * @brief  执行单次温度测量并返回摄氏温度值
 * @param  hi2c I2C句柄指针(例如 &hi2c1)
 * @param  temperature 存储摄氏温度值的指针
 * @param  timeout_ms 等待转换的最大时间(毫秒)
 * @retval T117_StatusTypeDef 成功返回T117_OK, I2C失败返回T117_ERROR, 转换超时返回T117_TIMEOUT
 */
T117_StatusTypeDef T117_Get_Temperature(I2C_HandleTypeDef *hi2c, float *temperature, uint32_t timeout_ms) {
    if (hi2c == NULL || temperature == NULL) {
        return T117_ERROR;
    }

    T117_StatusTypeDef status;
    int16_t raw_temp;

    // 1. 启动单次转换
    status = T117_Start_Single_Conversion(hi2c);
    if (status != T117_OK) {
        return status;
    }

    // 2. 等待转换完成
    status = T117_Wait_For_Conversion(hi2c, timeout_ms);
    if (status != T117_OK) {
        return status; // 可能是超时或错误
    }

    // 3. 读取原始温度
    status = T117_Read_Raw_Temperature(hi2c, &raw_temp);
    if (status != T117_OK) {
        return status;
    }

    // 4. 转换为摄氏度
    *temperature = T117_Convert_Raw_To_Celsius(raw_temp);

    return T117_OK;
}