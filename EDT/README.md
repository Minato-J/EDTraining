<div align="center">

# 🏎️ 智能循迹小车 — 2026 全国大学生电子设计竞赛

**H0 赛题 · 武汉工程大学校内选拔赛**

[![MCU](https://img.shields.io/badge/MCU-STM32F407VET6-03234B?logo=stmicroelectronics)](https://www.st.com)
[![IDE](https://img.shields.io/badge/IDE-Keil%20MDK-00A651)](https://www.keil.com)
[![Language](https://img.shields.io/badge/Lang-C11%20%2F%20MicroPython-555)](.)
[![K230](https://img.shields.io/badge/Vision-K230_Camera-FF6A00)](.)

**视觉循迹 + 陀螺仪姿态保持 + 双环 PID 闭环控制**

</div>

---

## 📖 一句话

> STM32F407 + K230 摄像头 + JY901S 陀螺仪 → 双轮差速小车沿指定赛道自动行驶。5 个赛道任务，覆盖直线 / 弧段 / 对角线 / 多圈连续行驶。

---

## 🎯 赛道任务

| # | 路线 | 圈数 | 约束 | 状态 |
|:--:|------|:--:|------|:--:|
| **1** | A → B（直线） | — | B 点停车 + 声光提示 | ✅ |
| **2** | A → B → C → D → A（外圈） | 1 | 每节点声光提示 | ✅ |
| **3** | A → C → B → D → A（对角线） | 1 | 最少停车次数 | ✅ |
| **4** | A → C → B → D → A | 3 | 恰好停车 4 次 | ✅ |
| **5** | A → C → B → D → A | 4 | 扩展保留 | 🔧 调试中 |

---

## 🛠️ 技术栈

<table>
<tr>
<td width="120"><strong>主控</strong></td>
<td>STM32F407VET6 — Cortex‑M4 @ 168 MHz，全部实时控制逻辑</td>
</tr>
<tr>
<td><strong>视觉</strong></td>
<td>K230 摄像头 — 6 路灰度循迹，USART3 @ 115200 bps</td>
</tr>
<tr>
<td><strong>陀螺仪</strong></td>
<td>JY901S — Yaw 角 + 角速度，USART2 @ 9600 bps</td>
</tr>
<tr>
<td><strong>驱动</strong></td>
<td>TB6612FNG 双 H 桥 — TIM3 PWM（CH1 左 / CH2 右）</td>
</tr>
<tr>
<td><strong>反馈</strong></td>
<td>编码器 × 2 — TIM2（左）/ TIM4（右）编码器模式</td>
</tr>
<tr>
<td><strong>显示</strong></td>
<td>0.96" OLED — PB6/PB7 软件 I2C</td>
</tr>
<tr>
<td><strong>交互</strong></td>
<td>PB0（切任务）· PB1（启动）</td>
</tr>
<tr>
<td><strong>节拍</strong></td>
<td>TIM6 ~20 ms 中断 → PID 控制周期</td>
</tr>
</table>

---

## 🧠 控制架构

```
TIM6 ISR (20ms)
  │
  ├─ 1. 节点去抖 (200ms)       → 边沿检测 count++
  ├─ 2. 角度突变过滤 (>30° 丢弃)
  ├─ 3. 旋转优先判断 (rotating?)
  │      ├─ rotating=1        → 纯 IMU 角度环原地旋转
  │      ├─ K230=0x00 丢线    → 盲开模式 (角度环锁定)  + 1s 超时保护
  │      └─ K230 在线         → 循迹模式 (K230 转弯量修正)
  ├─ 4. 差速合成              → L = base + ff_diff - turn
  │                              R = base - ff_diff + turn
  └─ 5. 速度环 PID            → 增量式 × 2 (左/右独立闭环)
```

### PID 参数

| 控制环 | Kp | Ki | Kd |
|--------|:--:|:--:|:--:|
| 左轮速度环 | 0.9 | 0.12 | — |
| 右轮速度环 | 0.8 | 0.12 | — |
| 角度环 | 1.2 | — | 0.6 |

---

## 🏟️ 场地

```
         A ──────────────── B          ← 顶部直线 100 cm
        ╱                    ╲
       ╱                      ╲        ← 左右圆弧 R=40 cm
      │                        │
       ╲                      ╱
        ╲                    ╲
         D ──────────────── C          ← 底部直线 100 cm

    外框: 220 × 120 cm  |  仅可前进，不可后退
```

---

## 📁 仓库结构

```
├── H0/Core/                   # STM32CubeMX HAL 层
│   ├── Inc/                   # main.h · tim.h · usart.h · gpio.h
│   └── Src/main.c             # ★ 核心控制逻辑 (TIM6 ISR)
├── H0/Drive/                  # ★ 手写驱动模块
│   ├── Motor.c/h              #   TB6612 PWM 驱动
│   ├── encoder.c/h            #   编码器 + 低通滤波
│   ├── pid.c/h                #   增量式 PID
│   ├── wit_imu.c/h            #   JY901S UART 解析
│   ├── k230_track.c/h         #   K230 视觉转弯量计算
│   ├── task.c/h               #   任务调度 + 按键扫描
│   └── oled.c/h               #   OLED 显示
├── H0/untitled_1.py           # K230 MicroPython 端
├── H0/Drivers/                # ST 官方 HAL 库
├── H0/MDK-ARM/                # Keil 工程文件
├── Analysis/                  # 分析报告
│   └── H0_Analysis_Report.md  # 📖 最权威参考文档
├── document/                  # 赛题资料 & 设计报告
├── issues/                    # 已知问题跟踪
└── CLAUDE.md                  # AI 辅助开发指南
```

---

## ⚡ 快速开始

### 1. 编译烧录

```
Keil MDK → 打开 H0/MDK-ARM/*.uvprojx → 编译 → ST-Link/J-Link 烧录
```

### 2. K230 部署

将 `H0/untitled_1.py` 上传至 K230 开发板运行，UART3 连接 STM32。

### 3. 操作

| 步骤 | 操作 | 现象 |
|:--:|------|------|
| 1 | 上电 | OLED 显示 Task 编号 |
| 2 | 按 **PB0** 切换 | 循环 1→2→3→4→5 |
| 3 | 摆好起始位置 | A 点，车头对准路线 |
| 4 | 按 **PB1** 启动 | 重置状态，开始行驶 |

---

## 🔌 通信协议

| 外设 | 接口 | 波特率 | 帧格式 |
|------|------|:--:|------|
| K230 → STM32 | USART3 | 115200 | `0xAA` `0x55` `[data]` — bit0~5 为 6 路灰度 |
| JY901S → STM32 | USART2 | 9600 | 11 字节 `0x55 + type + ... + checksum` |
| STM32 → 调试 | USART1 | 115200 | `printf()` 调试输出 |

---

## 📝 最近更新

| 日期 | 内容 |
|------|------|
| 2026‑06‑03 | K230 保守参数 · 旋转屏蔽 · 盲开超时保护 · stray `t` 修复 |
| 2026‑05 | Task 4（不停车 3 圈）合并到主线 · 节点去抖 · 前馈差速 |
| 2026‑05 | 初始版本：Task 1~3 框架搭建 |

> 详见 [commits](https://github.com/Minato-J/EDTraining/commits/main) 和 `issues/` 目录。

---

## ⚠️ 已知问题

| # | 问题 | 状态 |
|:--:|------|:--:|
| [006](issues/006-sound-light-alert.md) | 声光提示系统 | 🔴 待实现 |
| [007](issues/007-k230-params-safety.md) | K230 参数安全加固 | ✅ 已修复 |
| [008](issues/008-blind-overtime-stop.md) | 盲开超时紧急停车 | ✅ 已修复 |
| [009](issues/009-rotation-k230-mask.md) | 旋转阶段 K230 屏蔽 | ✅ 已修复 |

---

## 📚 参考文档

- **[完整分析报告](Analysis/H0_Analysis_Report.md)** — 赛题 · 硬件 · 架构 · 算法 全解析
- **[设计报告](document/设计报告_基于视觉巡线的自动行驶小车.docx)** — 正式提交文档
- **[电赛培训介绍](document/电赛培训介绍.html)** — 赛前培训资料
- **[旧版参考](document/example/)** — STM32F1 小车代码（仅供参考）

---

<div align="center">

**Minato-J** · 2026 全国大学生电子设计竞赛

</div>
