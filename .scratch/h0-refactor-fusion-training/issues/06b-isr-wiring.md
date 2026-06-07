# 06b — ISR 接入 Car_ControlLoop

**Status:** ready-for-agent

## Parent

Issue 06 — ctrl_mode 显式控制模式大重构

## What to build

修改 `Core/Src/main.c`，将 TIM6 ISR 中的 ~50 行双模切换逻辑替换为 `Car_ControlLoop()` 调用。

### main.c 具体改动

1. `#include "car_control.h"` 新增
2. 删除变量定义：`blind_ticks`, `turn_out_smooth`, `turn_out`, `target_angle`, `base_speed`（已移入 car_control.c）
3. 删除 `extern uint8_t rotating;`（不再需要）
4. `ControlState_Reset()` 中删除 `blind_ticks=0; turn_out_smooth=0;`，改为 `Car_ResetState();`
5. `main()` 初始化中添加 `Car_Init();`
6. ISR 控制段改为一句话：

```c
// 旧代码 (~50 行):
if (task_running == 1) {
    if (rotating) { ... }
    else if (line_sensor_data == 0x00 || !k230_data_valid) { ... }
    else { ... }
    turn_out_smooth = ...;
    target_v_left = ...;
    target_v_right = ...;
} else { target_v_left = 0; target_v_right = 0; }

// 新代码 (1 行):
Car_ControlLoop();
```

ISR 中保留不动：角度突变过滤器、节点计数、编码器滤波、速度 PID、电机输出。

## Acceptance criteria

- [ ] 编译零错误零警告
- [ ] ISR 代码行数减少 ~50 行
- [ ] 控制行为与改造前完全一致（逻辑只是搬家，不改算法）
- [ ] 盲开超时保护仍然生效
- [ ] turn_out 平滑滤波仍然生效
- [ ] Task_Dispatcher 中 `base_speed = 0` 改为 `Car_Stop()`

## Blocked by

- [06a-car-control-module] — 需要 car_control 模块先存在
