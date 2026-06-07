# Issue 08c: ISR 瘦身 — 删除 ISR 内 Car_ControlLoop + 速度环 PID + Motor PWM

## Parent

Issue 08 — TIM6 ISR 卸载

## What

在 08b 验证主循环消费块工作正常后，删除 ISR 中已被主循环覆盖的冗余代码。ISR 只保留采集职责。

### ISR 删除清单

删除以下代码（位置参考 main.c TIM6 ISR 末尾）：

1. `Car_SensorInputs in = {...};`  — 快照组装（已移到 ISR 快照区）
2. `Car_ControlOutputs out = Car_ControlLoop(in);` — 控制决策
3. `target_v_left  = out.target_v_left;` — 左轮目标速度赋值
4. `target_v_right = out.target_v_right;` — 右轮目标速度赋值
5. `if (out.emergency_stop) task_running = 0;` — 紧急停车
6. 左轮速度环整块：`raw_l = Encoder_Get_Count_Left()` / `fil_l` 滤波 / `PID_Compute` / `Motor_SetSpeed_A` / `current_v_left`
7. 右轮速度环整块：`raw_r = Encoder_Get_Count_Right()` / `fil_r` 滤波 / `PID_Compute` / `Motor_SetSpeed_B` / `current_v_right`
8. `debug_tick_cnt++`（移到主循环消费块内）

### ISR 保留清单

- count_debounce 去抖 + 边沿检测 → count++
- K230_Timeout_Tick()
- ReachPoint_Tick()
- 角度突变过滤器 → current_angle
- YawTrack_Update(current_angle)
- **快照区**（08a 新增）：编码器原始值 + Car_SensorInputs 组装 + isr_control_ready = 1

### 同步调整

**ControlState_Reset()**：主循环消费块的 `fil_l`/`fil_r` 是 `static`，保留最后一帧值。但 Start 时 Task_Key_Scan 已调用 `ControlState_Reset()` 重置 PID。需增加：

```c
// ControlState_Reset() 内新增
extern float g_fil_l, g_fil_r;  // 从 ISR static 提升为文件级
g_fil_l = 0;
g_fil_r = 0;
```

→ 注意：需要先把主循环消费块中的 `static float fil_l/fil_r` 提升为文件级 `g_fil_l/g_fil_r`，使 ControlState_Reset() 能访问。这个提升在本 issue 做（08b 保持 static 是为了最小改动先验证）。

**debug_tick_cnt++**：从 ISR 移到主循环消费块末尾。

### ISR 最终形态（~45 行）

```
if (htim->Instance == TIM6) {
    if (task_running) {
        count_debounce / 边沿检测 → count++
        K230_Timeout_Tick()
        ReachPoint_Tick()
    }
    角度突变过滤器 → current_angle
    YawTrack_Update(current_angle)
    // 快照
    isr_snapshot_raw_l = Encoder_Get_Count_Left()
    isr_snapshot_raw_r = Encoder_Get_Count_Right()
    isr_snapshot_in = {current_angle, line_sensor_data, k230_data_valid}
    isr_control_ready = 1
}
```

### 删除代码量

约 30 行（从 ~80 行 → ~45 行）

## Acceptance criteria

- [ ] 编译 0 errors 0 warnings
- [ ] ISR 不再调用 `Car_ControlLoop` / `PID_Compute` / `Motor_SetSpeed`
- [ ] 主循环消费块工作正常（已在 08b 验证）
- [ ] `ControlState_Reset()` 清零 `g_fil_l` / `g_fil_r`
- [ ] `debug_tick_cnt++` 在主循环消费块内
- [ ] 实物测试：Task 1/2/3/4 行为与 08b 验证结果一致

## Blocked by

- Issue 08b（需实物验证主循环消费块工作正常后才能删 ISR 代码）
