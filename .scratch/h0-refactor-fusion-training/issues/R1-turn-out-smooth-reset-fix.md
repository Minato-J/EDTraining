# R1 — Task 1/2/3/5 Car_StartLine 每 tick 清零 turn_out_smooth 修复

**Status:** ready-for-agent
**Severity:** 🔴 P0

## Parent

Issue 06c/06d — Task 1/2/5 和 Task 3 迁移到 car_control API

## What to build

Count 驱动型 Task（1/2/3/5）的每个 case 分支每 10ms 都被 Task_Dispatcher 调用一次。当前使用 `Car_StartLine(angle, speed, diff)` 导致每 tick 执行 `turn_out_smooth = 0.0f`，低通滤波 `0.3*old + 0.7*new` 永远以 0 作为历史值，平滑效果完全消失。

修复：将 Task 1/2/3/5 中每 tick 调用的 `Car_StartLine()` 替换为独立 setter 组合，只在需要时设参数，不重置累积状态。

### 改动对照

**Task 1**: `Car_StartLine(0.0f, 60, 0)` → `Car_SetTargetAngle(0.0f); Car_SetSpeed(60); Car_SetFFDiff(0);`

**Task 2**: 每个 case 的 Car_StartLine → 三行独立 setter

**Task 3**: 弯道刹车段的 Car_StartLine → Car_SetTargetAngle + Car_SetSpeed（t3_wait_tick 三元）+ Car_SetFFDiff(0)

**Task 5**: 每个 if-else 分支的 Car_StartLine → 三行独立 setter，停车用 Car_Stop

### Task 3 case 1 刹车修复示例

```c
// 旧（错误）:
Car_StartLine(0.0f, (t3_wait_tick <= TICK_IN_BRAKE) ? SPEED_IN_BRAKE : SPEED_CURVE_RUN, 0);

// 新（正确）:
Car_SetTargetAngle(0.0f);
t3_wait_tick++;
Car_SetSpeed((t3_wait_tick <= TICK_IN_BRAKE) ? SPEED_IN_BRAKE : SPEED_CURVE_RUN);
Car_SetFFDiff(0);
```
注意：`t3_wait_tick++` 移到 `Car_SetSpeed()` 之前，因为原代码中 `t3_wait_tick++` 在 `target_angle` 赋值之后、`base_speed` 赋值之前。

## Acceptance criteria

- [ ] Task 1/2/3/5 不再每 tick 重置 turn_out_smooth
- [ ] 低通滤波恢复正常平滑效果
- [ ] 弯道刹车计数 t3_wait_tick++ 位置正确
- [ ] 盲开超时计数器 blind_ticks 不会被每 tick 清零（可能产生虚假的超时延迟）
- [ ] 编译零错误零警告

## Blocked by

None - can start immediately
