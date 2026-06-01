# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

2026 年全国大学生电子设计竞赛（电赛）智能小车项目。使用 STM32F407VET6 作为主控，K230 摄像头模组进行视觉循迹，JY901S 陀螺仪做姿态保持，通过 TB6612 驱动双轮差速小车沿指定路线自动行驶。

## 构建与开发

- **IDE**: Keil MDK (ARM Compiler 5/6)，工程文件在 `H0/MDK-ARM/` 下
- 没有命令行构建工具，需通过 Keil IDE 编译烧录
- **K230 端**: `H0/untitled_1.py` — MicroPython 程序，运行在 K230 开发板上，负责 6 路灰度循迹并通过 UART3(115200bps) 发送数据给 STM32

## 代码架构

```
H0/
├── Core/Src/main.c     核心逻辑：外设初始化 + 主循环 + TIM6 中断控制
├── Drive/              用户自定义驱动层（手写模块）
│   ├── Motor.c/h       TB6612 电机驱动 (TIM3 PWM)
│   ├── encoder.c/h     编码器读取 (TIM2/TIM4)
│   ├── pid.c/h         增量式 PID 控制器
│   ├── wit_imu.c/h     JY901S 陀螺仪解析 (UART2 字节状态机)
│   ├── k230_track.c/h  K230 视觉数据解析与转弯量计算
│   ├── task.c/h        任务管理 (按键扫描 + 状态机调度)
│   └── oled.c/h        OLED 显示 (软件 I2C, PB6/PB7)
├── Drivers/            ST 官方 HAL 库 (CMSIS + STM32F4xx_HAL)
└── MDK-ARM/            Keil 工程文件
```

### 控制架构 (TIM6 中断，~20ms 周期)

main.c 的 `HAL_TIM_PeriodElapsedCallback(TIM6)` 包含全部实时控制逻辑，执行顺序：

1. **节点计数去抖** — 检测 `line_sensor_data` 从有线→无线的边沿，带 200ms 去抖窗口
2. **角度突变过滤** — 20ms 内 Yaw 变化 >30° 时丢弃读数，保留上次有效值
3. **双模切换**：
   - `line_sensor_data == 0x00`（全白/丢线）→ **盲开模式**：角度环 PID 保持 `target_angle`
   - 否则 → **循迹模式**：`K230_Get_Turn_Speed()` 输出转弯量，同时同步 `target_angle = current_angle` 防止切回盲开时抖动
4. **turn_out 平滑** — 一阶低通 `0.3 * old + 0.7 * new`
5. **差速合成** — `left = base_speed + ff_diff - turn_out`，`right = base_speed - ff_diff + turn_out`
6. **速度环 PID** — 左右轮各自独立闭环（增量式 PID + 编码器低通滤波）

### 两种导航策略

Task 1/2/5 使用 **count 驱动**：节点计数器 `count` 直接决定当前路段（直线/弧段），每个 case 设置 `target_angle` + `base_speed` + `ff_diff`。简单但依赖节点检测可靠性。

Task 3/4 使用 **航点状态机**：`current_state` 驱动多状态流转，每个状态包含旋转（base_speed=0，设 target_angle）和推进（base_speed>0，等 count 触发下一状态）两个阶段。Task 3 每节点有 `wait_tick` 等待旋转完成，Task 4 注释掉等待实现不停车。

### 关键全局变量（main.c ↔ task.c 共享）

| 变量 | 说明 |
|------|------|
| `target_angle` | 陀螺仪目标航向角（盲开模式锁定此值） |
| `base_speed` | 基础前进速度（直线 60，弧段 30） |
| `ff_diff` | 前馈差速（弧段时给内轮减速、外轮加速） |
| `count` | 节点计数器（边沿检测触发） |
| `task_running` | 任务运行标志（1=运行，0=停车） |
| `current_state` | 航点状态机当前状态 |
| `line_sensor_data` | K230 发来的 6 路灰度数据（bit0~bit5） |

### K230 通信协议

UART3 接收，帧格式：`0xAA 0x55 [data]`，data 的 bit0~bit5 对应 6 路灰度传感器（1=检测到黑线）。`K230_Get_Turn_Speed()` 将传感器值映射到转弯量（±3~±27），0x00 时保持上次转弯量。

### 按键操作

- **PB0**（Shift）：切换任务编号（1~5），仅在停车时有效
- **PB1**（Start）：记录当前 Yaw 为 `Yaw_Offset`，重置所有状态，开始运行

### 赛题-任务映射

| Task | 赛题 | 路线 | 导航策略 |
|------|------|------|----------|
| 1 | 基本要求(1) | A→B 直线 | count 驱动 |
| 2 | 基本要求(2) | A→B→C→D→A 外圈 | count 驱动 + ff_diff |
| 3 | 发挥(3) | A→C→B→D→A 1圈 | 9 状态航点（每节点停车旋转） |
| 4 | 发挥(4) | A→C→B→D→A 3圈 | 29 状态航点（不停车） |
| 5 | 扩展保留 | A→C→B→D→A ×4圈 | count 驱动 |

### 场地参数

- 外框：220cm × 120cm
- 圆弧半径：40cm，左右对称
- 顶部直线 A→B：100cm
- 对角线方向角：约 ±38°（代码中 `-38.0f`）和 ±144°（代码中 `-144.0f`）

### 关键参考文档

- **Analysis/H0_Analysis_Report.md** — 最权威的项目分析报告
- **issues/** — 已知问题跟踪（K230 参数安全、盲开超时、旋转遮罩等）
