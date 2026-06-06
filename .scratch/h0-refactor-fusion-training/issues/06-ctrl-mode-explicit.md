# 06 — ctrl_mode 显式控制模式大重构

**Status:** ready-for-human

## Parent

`.docs/Analysis/底层重构实施计划_H0_fusion_training.md` — Slice F

## What to build

新建 `Drive/car_control.c/h`，将当前 TIM6 ISR 中 100+ 行的分散控制逻辑收敛为 4 种显式控制模式 + 统一 API。这是本次重构中影响面最大的切片。

**4 种控制模式**：

| 模式 | 说明 | 控制策略 |
|------|------|----------|
| `CTRL_PARK` (0) | 停车 | PWM=0，电机停转，清零累加器 |
| `CTRL_STRAIGHT` (1) | 直行 | 增量式速度 PI（左右轮同步）+ 小角度 PD 补偿（Kp=1.5, Kd=0.3, comp 限幅 ±10），完全不依赖 K230 |
| `CTRL_LINE` (2) | 循迹 | K230 转弯量 + 双模切换（丢线→盲开角度保持），保留 H0 全部安全机制 |
| `CTRL_TURN` (3) | 原地旋转 | 比例 P 控制（Kp=4.0）+ ±3° 死区 + 最小 PWM=125（克服静摩擦），左右轮差速反转 |

**API 设计**：

```c
void Car_ControlLoop(void);        // 替代 TIM6 ISR 中的分散逻辑，每 20ms 调用
void Car_SetMode(ctrl_mode_t m);   // 模式切换 + 自动清零相关累加器
void Car_TurnTo(float target);     // CTRL_TURN 封装
void Car_StartStraight(void);      // CTRL_STRAIGHT 封装（锁定当前 yaw 为目标）
void Car_StartLine(void);          // CTRL_LINE 封装
void Car_Stop(void);               // CTRL_PARK 封装
```

**改动链路**：新建 car_control.c（4 模式状态机 + API）→ main.c TIM6 ISR 从 ~120 行精简到 ~30 行（角度过滤 → Car_ControlLoop → 电机输出）→ task.c 5 个 Task 改为 API 调用（Car_StartStraight / Car_TurnTo / Car_StartLine / Car_Stop），一条完整的垂直切片。

**参考实现**：`training/hardware/CarDrive.c` ctrl_mode 体系 + API 函数。H0 保留所有安全机制（角度过滤器、编码器低通、turn_out 平滑、盲开超时、NaN 防护、JY901S 校验）作为 Car_ControlLoop 的内置能力。

**CTRL_STRAIGHT 是 H0 的新能力**：在起点到第一个黑线节点之间，不再依赖 K230/盲开二选一，而是用速度 PI + 角度 PD 主动走直线。training 已验证此模式有效。

## Acceptance criteria

- [ ] 所有 5 个 Task 行为与改造前一致（逐 Task 回归实测）
- [ ] CTRL_STRAIGHT 模式下小车走直线，航向偏差 < ±5°
- [ ] CTRL_TURN 旋转到位精度 ±3°
- [ ] 模式切换无抽搐、无突变（API 内部自动清零相关累加器）
- [ ] TIM6 ISR 执行时间不增加（精简后的 ControlLoop 不比重构前的分散逻辑慢）

## Blocked by

- [01-velocity-pi-incremental] — 增量式 PI 是 CTRL_STRAIGHT 直行模式的基础
