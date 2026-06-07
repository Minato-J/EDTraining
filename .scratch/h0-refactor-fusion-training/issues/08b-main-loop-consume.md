# Issue 08b: 主循环新增消费块 — Car_ControlLoop + 速度环 PID 从 ISR 复制到主循环

## Parent

Issue 08 — TIM6 ISR 卸载

## What

在 main() while(1) 中新增 `if (isr_control_ready)` 消费块，并行运行 ISR 原有和主循环新增两套控制逻辑（结果相同，输入相同）。

**关键设计**：本步骤实现逻辑复制而非逻辑移动。ISR 和主循环各自独立执行 Car_ControlLoop + 速度环 PID，主循环的输出覆盖 ISR 的输出（后写覆盖先写）。这保证 Motor PWM 最终的写入来自主循环，但 ISR 保留作为安全网。

### 主循环新增位置

`Task_Dispatcher()` 之后、OLED 刷新之前：

```c
// Issue 08b: 消费 ISR 快照 — Car_ControlLoop + 双轮速度环 PID
if (isr_control_ready) {
    Car_ControlOutputs out = Car_ControlLoop(isr_snapshot_in);
    target_v_left  = out.target_v_left;
    target_v_right = out.target_v_right;
    if (out.emergency_stop) task_running = 0;

    // 左轮速度环
    static float fil_l = 0;
    fil_l = 0.7f * fil_l + 0.3f * (float)isr_snapshot_raw_l;
#ifdef PID_USE_VELOCITY_FORM
    float out_l = PID_Velocity_Compute(&pid_vel_left, target_v_left, (int)fil_l);
#else
    float out_l = PID_Compute(&pid_left, (float)target_v_left, fil_l);
#endif
    Motor_SetSpeed_A((int16_t)out_l);
    current_v_left = (int16_t)fil_l;

    // 右轮速度环
    static float fil_r = 0;
    fil_r = 0.7f * fil_r + 0.3f * (float)isr_snapshot_raw_r;
#ifdef PID_USE_VELOCITY_FORM
    float out_r = PID_Velocity_Compute(&pid_vel_right, target_v_right, (int)fil_r);
#else
    float out_r = PID_Compute(&pid_right, (float)target_v_right, fil_r);
#endif
    Motor_SetSpeed_B((int16_t)out_r);
    current_v_right = (int16_t)fil_r;

    isr_control_ready = 0;
}
```

### 不改

- ISR 中 Car_ControlLoop / 速度环 PID / Motor_SetSpeed 全部保留
- fil_l/fil_r 的 ISR 版本和主循环版本各自独立（都从同一原始值快照计算，结果一致）
- Car_ControlLoop 签名不变
- 不添加 ControlState_Reset 中的 fil 清零（主循环 static fil 已在首次 Start 后自然重建）

## Acceptance criteria

- [ ] 编译 0 errors 0 warnings
- [ ] 主循环有 `if (isr_control_ready)` 消费块
- [ ] ISR 原有逻辑未变
- [ ] 实物测试：所有 Task 行为不变（主循环覆盖 ISR 输出，结果相同）

## Blocked by

- Issue 08a（需 isr_control_ready + 快照变量存在）
