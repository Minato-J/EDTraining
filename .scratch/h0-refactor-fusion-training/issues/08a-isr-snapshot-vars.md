# Issue 08a: ISR 快照基础设施 — 新增 volatile 传递变量 + ISR 末尾设 flag

## Parent

Issue 08 — TIM6 ISR 卸载

## What

在 main.c 新增 4 个 volatile 变量用于 ISR→主循环数据传递，并在 ISR 末尾填充快照 + 置 flag。**ISR 原有逻辑完全不动**——本步骤只是多写一份快照，零行为变化。

### 新增变量 (main.c PTD 区)

```c
// Issue 08a: ISR→主循环 数据传递
volatile uint8_t   isr_control_ready = 0;    // ISR 置1，主循环消费后清0
volatile Car_SensorInputs isr_snapshot_in;     // ISR 写入的角度/传感器快照
volatile int16_t   isr_snapshot_raw_l = 0;    // ISR 写入的左编码器原始值
volatile int16_t   isr_snapshot_raw_r = 0;    // ISR 写入的右编码器原始值
```

### ISR 末尾新增 (紧接 YawTrack_Update 之后)

```c
// Issue 08a: 快照编码器原始值 + 传感器输入，供主循环消费
isr_snapshot_raw_l = Encoder_Get_Count_Left();
isr_snapshot_raw_r = Encoder_Get_Count_Right();
isr_snapshot_in.current_angle   = current_angle;
isr_snapshot_in.line_sensor_data = line_sensor_data;
isr_snapshot_in.k230_data_valid  = k230_data_valid;
isr_control_ready = 1;
```

### 不改

- ISR 中 Car_ControlLoop / 速度环 PID / Motor_SetSpeed 全部保留
- 主循环不变
- 所有其他文件不变

## Acceptance criteria

- [ ] 编译 0 errors 0 warnings
- [ ] 新增 4 个 volatile 变量
- [ ] ISR 末尾有快照 + flag 赋值
- [ ] 实物测试：所有 Task 行为与改动前完全一致（快照被写但未被读）

## Blocked by

None
