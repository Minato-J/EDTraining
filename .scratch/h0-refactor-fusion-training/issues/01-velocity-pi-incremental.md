# 01 — 速度环增量式 PI 改造

**Status:** ready-for-agent

## Parent

`.docs/Analysis/底层重构实施计划_H0_fusion_training.md` — Slice A

## What to build

将 H0 左/右轮速度环的 PID 算法从**位置式**改为**增量式（velocity-form）PI**。角度环 PD 保持位置式不变。

增量式 PI 公式（参考 `training/hardware/CarDrive.c` velocity_pi）：

```
bias = target_encoder - measured_encoder
pwm += Kp * (bias - last_bias) + Ki * bias
clamp(pwm, 0, MAX)
last_bias = bias
```

**为什么**：增量式 PI 模式切换天然平滑（只依赖相邻两拍偏差），天然抗积分饱和。training 已在实际场地验证。

**设计决策**：
- 新旧两种算法共存于 pid.c，角度环继续用 `PID_Compute()`（位置式 PD），速度环专用 `PID_Velocity_Compute()`（增量式 PI）
- 编译期宏 `PID_USE_VELOCITY_FORM` 控制切换，未定义时保持位置式（零风险回退）
- 初始参数建议 Kp=15.0, Ki=3.0（取 training 值的 75% 保守起步，实车再精调）

**改动链路**：pid.c 算法层 → main.c TIM6 ISR 调用层 → task.c 状态清零层，一条完整的垂直切片。

## Acceptance criteria

- [ ] 小车直线行驶无速度振荡
- [ ] 模式切换（盲开↔循迹，节点触发）平滑无抽搐
- [ ] 停车后重新启动，PWM 不从历史残留值起步（ControlState_Reset 清零累加器）
- [ ] Task 1 A→B 直线完成时间与改造前差异 < 10%
- [ ] `PID_USE_VELOCITY_FORM` 未定义时编译行为与改造前完全一致

## Blocked by

None — 可立即开始
