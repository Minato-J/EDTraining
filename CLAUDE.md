# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

2026 年全国大学生电子设计竞赛（电赛）智能小车项目。使用 STM32F407VET6 作为主控，K230 摄像头模组进行视觉循迹，JY901S 陀螺仪做姿态保持，通过 TB6612 驱动双轮差速小车沿指定路线自动行驶。

## 构建与开发

- **IDE**: Keil MDK (ARM Compiler 5/6)，工程文件在 `H0/MDK-ARM/` 下
- **主控代码**：`H0/Core/Src/main.c`，包含外设初始化、主循环和 TIM6 中断中的全部控制逻辑（双模切换、速度环/角度环 PID）
- **K230 端**: `H0/untitled_1.py` — MicroPython 程序，运行在 K230 开发板上，负责 6 路灰度循迹并通过 UART3(115200bps) 发送数据给 STM32
- 没有命令行构建工具，需通过 Keil IDE 编译烧录

## 代码架构

```
H0/
├── Core/               STM32CubeMX 生成的 HAL 层代码
│   ├── Inc/            main.h, tim.h, usart.h, gpio.h, stm32f4xx_it.h
│   └── Src/            main.c (核心逻辑), tim.c, usart.c, gpio.c, stm32f4xx_it.c
├── Drive/              用户自定义驱动层（手写模块）
│   ├── Motor.c/h       TB6612 电机驱动 (TIM3 PWM)
│   ├── encoder.c/h     编码器读取 (TIM2/TIM4)
│   ├── pid.c/h         增量式 PID 控制器
│   ├── wit_imu.c/h     JY901S 陀螺仪解析 (UART 字节状态机)
│   ├── k230_track.c/h  K230 视觉数据解析与转弯量计算
│   ├── task.c/h        任务管理 (按键扫描 + 状态机调度)
│   └── oled.c/h        OLED 显示 (软件 I2C, PB6/PB7)
├── Drivers/            ST 官方 HAL 库 (CMSIS + STM32F4xx_HAL)
└── MDK-ARM/            Keil 工程文件
```

### 控制架构 (main.c TIM6 中断，~20ms 周期)

1. **双模切换策略**: `line_sensor_data == 0x00` 时走盲开模式（陀螺仪角度环保持），否则走循迹模式（K230 偏差量修正航向）
2. **增量式 PID**: 左轮速度环 (Kp=0.9, Ki=0.12)、右轮速度环 (Kp=0.8, Ki=0.12)、角度环 (Kp=1.2, Kd=0.6, 无 Ki)
3. **节点计数**: 检测黑线边沿变化 (`last_line_status` 状态翻转) 来计数经过的节点，驱动任务状态机阶段切换
4. **角度突变过滤器**: 20ms 内 Yaw 角变化超过 30° 时丢弃该读数

### 关键文件说明

- **Analysis/H0_Analysis_Report.md** — 完整的项目分析报告（赛题说明、硬件连接、软件架构、算法解析、完成进度、待完善方向），是最权威的参考文档
- **document/电赛培训介绍.html** — 电赛培训介绍网页
- **document/example/** — STM32F1 小车参考代码（旧版，仅供参考）
- **ZIP/** — 历史备份压缩包，已加入 .gitignore

### 任务完成进度

| 任务 | 状态 |
|------|------|
| 任务(1) A→B 直线 | 已实现 |
| 任务(2) A→B→C→D→A | 框架已搭，核心被注释 (~40%) |
| 任务(3) A→C→B→D→A | 空函数体 (~10%) |
| 任务(4) 精确停车次数 | 未开始 (0%) |
