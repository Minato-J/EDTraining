# 06c — Task 1/2/5 迁移到 car_control API

**Status:** ready-for-agent

## Parent

Issue 06 — ctrl_mode 显式控制模式大重构

## What to build

将 Task 1/2/5（count 驱动型）中的全局变量直接赋值改为 `Car_StartLine()` API 调用。

这三个 Task 迁移模式完全一致，批量处理。

### Task 1 改动

| 原代码 | 新代码 |
|--------|--------|
| `target_angle = 0.0f; base_speed = 60; ff_diff = 0;` | `Car_StartLine(0.0f, 60, 0);` |
| `task_running = 0; base_speed = 0;` | `Car_Stop(); task_running = 0;` |

### Task 2 改动

| case | 原代码 | 新代码 |
|------|--------|--------|
| 0 (A→B) | `target_angle=0; base_speed=60; ff_diff=0;` | `Car_StartLine(0.0f, 60, 0);` |
| 1 (B→C) | `target_angle=RIGHT_HALF_CIRCLE; base_speed=30; ff_diff=10;` | `Car_StartLine(RIGHT_HALF_CIRCLE, 30, 10);` |
| 2 (C→D) | `target_angle=180; base_speed=60; ff_diff=0;` | `Car_StartLine(180.0f, 60, 0);` |
| 3 (D→A) | `target_angle=LEFT_HALF_CIRCLE; base_speed=30; ff_diff=-10;` | `Car_StartLine(LEFT_HALF_CIRCLE, 30, -10);` |
| 4 (停车) | `task_running=0; base_speed=0; ff_diff=0;` | `Car_Stop(); task_running=0;` |

### Task 5 改动

| 路段 | 原代码 | 新代码 |
|------|--------|--------|
| count==0 | `target_angle=DIAG_AC_ANGLE; base_speed=60; ff_diff=0;` | `Car_StartLine(DIAG_AC_ANGLE, 60, 0);` |
| count==1 | `target_angle=RIGHT_HALF_CIRCLE; base_speed=30; ff_diff=10;` | `Car_StartLine(RIGHT_HALF_CIRCLE, 30, 10);` |
| count==2 | `target_angle=DIAG_BD_ANGLE; base_speed=60; ff_diff=0;` | `Car_StartLine(DIAG_BD_ANGLE, 60, 0);` |
| count==3 | `target_angle=LEFT_HALF_CIRCLE; base_speed=30; ff_diff=-10;` | `Car_StartLine(LEFT_HALF_CIRCLE, 30, -10);` |
| 停车 | `task_running=0; base_speed=0; ff_diff=0;` | `Car_Stop(); task_running=0;` |

Task 5 的圈数逻辑（lap_count++, count=0 循环）保持不变，只改参数赋值。

## Acceptance criteria

- [ ] 编译零错误零警告
- [ ] Task 1 A→B 直线行为不变
- [ ] Task 2 外圈行为不变
- [ ] Task 5 4圈循环行为不变

## Blocked by

- [06b-isr-wiring] — 需要 ISR 先接入 Car_ControlLoop
