# Issue 009: 旋转阶段 K230 屏蔽

## Parent

无 — 来自合并方案 §5.3

## What to build

当小车在路口原地旋转时（Task 3/4 的 `base_speed=0 + wait_tick` 阶段），通过 `rotating` 全局标志完全屏蔽 K230 视觉输入，纯 IMU 角度环控制旋转；旋转结束后恢复正常双模切换。

**横切三层**：`task.h`（声明）→ `task.c`（置位/清除）→ `main.c`（ISR 优先判断）

### 改动

1. **`H0/Drive/task.h`**: 新增 `extern uint8_t rotating;`
2. **`H0/Drive/task.c`**:
   - 新增 `uint8_t rotating = 0;`
   - `Run_Task_3` 旋转状态（state 0/2/4/6）进入时设 `rotating = 1`，退出时设 `rotating = 0`
   - `Run_Task_4` 旋转状态（state 0/2/4/6/10/12/14/16/20/22/24/26 — 已注释 wait_tick 的不停版本不需要设置，仅保留在终点 state 28）
3. **`H0/Core/Src/main.c`** — TIM6 ISR: 在双模切换最外层增加 `if (rotating)` 优先分支：

```c
extern uint8_t rotating;

if (rotating)
{
    // 旋转模式：纯 IMU 角度环控制，完全无视 K230
    float angle_error = target_angle - current_angle;
    while (angle_error > 180.0f)  angle_error -= 360.0f;
    while (angle_error < -180.0f) angle_error += 360.0f;
    turn_out = PID_Compute(&pid_angle, angle_error, 0);
}
else if (line_sensor_data == 0x00)
{
    // 盲开模式 ...
}
else
{
    // K230 循迹模式 ...
}
```

### 需设置 `rotating = 1` 的 Task 3 旋转状态

| State | 描述 | 旋转角度 |
|-------|------|---------|
| 0 | A点原地旋转 | -38° |
| 2 | C点原地扭头 | 0° |
| 4 | B点原地大调头 | -144° |
| 6 | D点原地回正 | 180° |

> Task 4 为不停车版本，无 wait_tick 旋转阶段，不需要设置 rotating。

## Acceptance criteria

- [ ] 编译 0 errors 0 warnings
- [ ] `task.h` 声明 `extern uint8_t rotating`
- [ ] `task.c` 定义 `rotating` 并在旋转状态正确置 1/0
- [ ] `main.c` ISR 增加了 `if (rotating)` 最外层优先分支
- [ ] 旋转分支使用纯 IMU 角度环控制，不调用 `K230_Get_Turn_Speed`
- [ ] 退出旋转后小车恢复正常 K230 循迹

## Blocked by

None — 可立即开始。建议在 Issue 008（盲开超时）之后实施，两者在 main.c 相邻区域。
