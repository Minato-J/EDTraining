# 06a — 新建 car_control 模块

**Status:** ready-for-agent

## Parent

Issue 06 — ctrl_mode 显式控制模式大重构

## What to build

新建 `Drive/car_control.h` 和 `Drive/car_control.c`，纯增量，不改任何现有文件。

### car_control.h

- `ctrl_mode_t` 枚举：`CTRL_PARK(0)`, `CTRL_STRAIGHT(1)`, `CTRL_LINE(2)`, `CTRL_TURN(3)`
- extern 全局变量：`target_angle`, `base_speed`, `ff_diff`（从 main.c/task.c 移入）
- API 声明：`Car_Init`, `Car_ControlLoop`, `Car_Stop`, `Car_StartLine`, `Car_StartStraight`, `Car_TurnTo`, `Car_SetSpeed`, `Car_SetTargetAngle`, `Car_SetFFDiff`, `Car_GetMode`, `Car_ResetState`

### car_control.c

实现 Car_ControlLoop()：4 模式分发 + CTRL_LINE 内部双模切换 + 盲开超时保护 + turn_out_smooth + 差速合成。

内部静态变量：`ctrl_mode`, `blind_ticks`, `turn_out_smooth`
全局变量（本模块拥有）：`target_angle`, `base_speed`, `ff_diff`
外部依赖（extern）：`current_angle`, `pid_angle`, `target_v_left`, `target_v_right`, `task_running`, `k230_data_valid`, `line_sensor_data`, `PID_Compute`, `K230_Get_Turn_Speed`

## Acceptance criteria

- [ ] 编译零错误零警告（模块独立编译通过）
- [ ] Car_ControlLoop() 逻辑与当前 main.c ISR 双模切换等价
- [ ] CTRL_PARK → target_v 清零
- [ ] CTRL_TURN → 纯角度 P 控制
- [ ] CTRL_LINE + 有线 → K230 循迹 + target_angle 同步
- [ ] CTRL_LINE + 丢线 → 盲开降级 + 1s 超时停车
- [ ] turn_out_smooth 0.3/0.7 低通 + ff_diff 差速合成保留

## Blocked by

- [01-velocity-pi] — CTRL_STRAIGHT 依赖增量式 PI
- [05-k230-weighted-average] — CTRL_LINE 使用加权平均法
