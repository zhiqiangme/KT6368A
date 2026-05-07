# STM32_Thermometer

基于 STM32F103 和 T117 温度传感器的蓝牙温度计，通过 KT6368A 蓝牙模块将温度数据发送至手机，配套 Android / HarmonyOS 双平台 APP。

项目地址：https://github.com/zhiqiangme/STM32_Thermometer

## 硬件概述

| 模块 | 型号 | 说明 |
|------|------|------|
| MCU | STM32F103 | 主控芯片 |
| 温度传感器 | T117 | I2C 接口，支持连续/单次转换、可配置采样率 |
| 显示屏 | 0.96" OLED | I2C 接口，实时显示温度 |
| 蓝牙模块 | KT6368A | SPP/BLE 双模，串口透传 |

## 项目结构

```
STM32_Thermometer/
├── STM32/          # STM32 固件（Keil 工程）
│   ├── BSP/        # 板级驱动（T117、OLED、I2C）
│   ├── Drivers/    # HAL 库与 CMSIS
│   ├── System/     # 系统工具（延时、串口）
│   └── User/       # 主程序
├── Android/        # Android APP（Kotlin）
├── Harmony/        # HarmonyOS APP（ArkTS）
└── .gitignore
```

## STM32 固件

- 开发环境：Keil MDK-ARM
- 目录：`/STM32`
- 主要功能：
  - T117 温度采集（I2C 软件模拟）
  - OLED 实时显示温度
  - 串口透传温度数据至 KT6368A 蓝牙模块

## Android APP

- 开发环境：Android Studio
- 目录：`/Android`
- 包名：`com.zhiqiangme.kt6368a`
- 最低 SDK：32（Android 12L）
- 主要功能：
  - BLE 扫描与连接
  - 接收蓝牙温度数据并显示
  - 通知栏常驻温度显示

## HarmonyOS APP

- 开发环境：DevEco Studio
- 目录：`/Harmony`
- 主要功能：
  - BLE 扫描与连接
  - 接收蓝牙温度数据并显示

## 许可证

本项目仅供学习参考。
