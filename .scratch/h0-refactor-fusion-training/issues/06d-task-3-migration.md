# 06d — Task 3 迁移到 car_control API

**Status:** ready-for-agent

## Parent

Issue 06 — ctrl_mode 显式控制模式大重构

## What to build

将 Task 3（count 驱动 + 进弯刹车/出弯缓冲）迁移到 car_control API。

### 关键点

Task 3 的刹车逻辑通过 `t3_wait_tick` 计数器动态调整 `base_speed`：
- 前 TICK_IN_BRAKE(10) ticks → base_speed = 0（进弯重刹）
- 之后 → base_speed = SPEED_CURVE_RUN(30)

迁移后，刹车逻辑仍在 task.c 中，通过 `Car_SetSpeed()` 实现节流：

```c
case 1: // C→B
    target_angle = 0.0f;
    t3_wait_tick++;
    Car_StartLine(0.0f,
        (t3_wait_tick <= TICK_IN_BRAKE) ? SPEED_IN_BRAKE : SPEED_CURVE_RUN,
        0);
    break;
```

或者使用 `Car_StartLine` + `Car_SetSpeed`：

```c
case 1:
    Car_StartLine(0.0f, SPEED_CURVE_RUN, 0);
    t3_wait_tick++;
    if (t3_wait_tick <= TICK_IN_BRAKE) {
        Car_SetSpeed(SPEED_IN_BRAKE);
    }
    break;
```

推荐第一种（一行搞定），因为 `Car_StartLine` 每次 tick 都被调用，每次都会覆盖 speed。

### 改动汇总

| case | 原逻辑 | 新逻辑 |
|------|--------|--------|
| 0 (A→C) | `target_angle=-38; base_speed=60; ff_diff=0;` | `Car_StartLine(-38.0f, 60, 0);` |
| 1 (C→B) | 刹车 + `target_angle=0; base_speed=刹车/30` | `Car_StartLine(0.0f, brake_or_30, 0);` |
| 2 (B→D) | 刹车 + `target_angle=-144; base_speed=刹车/30` | `Car_StartLine(-144.0f, brake_or_30, 0);` |
| 3 (D→A) | 刹车 + `target_angle=180; base_speed=刹车/30` | `Car_StartLine(180.0f, brake_or_30, 0);` |
| 4 (停车) | `task_running=0; base_speed=0; count=0; ...` | `Car_Stop(); task_running=0; count=0; ...` |

### t3_last_count / YawTrack 逻辑

保持不变。count 变化检测 + YawTrack_Reset + 弯道兜底自动 count++ 全部不动。

## Acceptance criteria

- [ ] 编译零错误零警告
- [ ] Task 3 对角线 1 圈完整跑通
- [ ] 进弯刹车/出弯缓冲行为不变
- [ ] 弯道 YawTrack 兜底仍然生效

## Blocked by

- [06c-task-1-2-5-migration] — 先完成简单 Task 验证基础框架正确
