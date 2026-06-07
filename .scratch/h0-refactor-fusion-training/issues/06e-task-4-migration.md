# 06e — Task 4 迁移到 car_control API

**Status:** ready-for-agent

## Parent

Issue 06 — ctrl_mode 显式控制模式大重构

## What to build

将 Task 4（29 状态航点、3 圈不停车）迁移到 car_control API。这是最复杂的 Task，因为它是唯一使用状态机而非 count 驱动的。

### 核心改动

Task 4 每个状态目前设置 2-3 个全局变量：
```c
case N:
    base_speed = X;
    target_angle = Y;
    ff_diff = 0;
    count = Z;
    YawTrack_Reset(current_angle);
    current_state = M;
    break;
```

迁移后改为一个 API 调用：
```c
case N:
    Car_StartLine(Y, X, 0);
    count = Z;
    YawTrack_Reset(current_angle);
    current_state = M;
    break;
```

### 不需要 Car_TurnTo 的场景

经过分析，Task 4 是"不停车"模式——航点切换时不旋转，只是改变 target_angle 然后继续循迹。所有状态都使用 `Car_StartLine()` 即可。

如果将来需要在某状态停车旋转，只需把 `Car_StartLine` 改为 `Car_TurnTo`。

### 改动模式（28 个有效状态 × 相同模式）

所有 `case` 分支中，三行赋值：
```c
base_speed = <speed>;
target_angle = <angle>;
ff_diff = 0;
```
替换为：
```c
Car_StartLine(<angle>, <speed>, 0);
```

每圈角度微调：
- 1 圈: -38°, 0°, -146°, 180°
- 2 圈: -34°, 0°, -148°, 180°
- 3 圈: -37°, 0°, -146°, 180°（每圈微调后）

### 停车状态 (case 28)

```c
// 原代码:
base_speed = 0;
task_running = 0;
current_state = 0;
count = 0;
wait_tick = 0;

// 新代码:
Car_Stop();
task_running = 0;
current_state = 0;
count = 0;
wait_tick = 0;
```

## Acceptance criteria

- [ ] 编译零错误零警告
- [ ] Task 4 三圈完整跑通
- [ ] 每圈角度微调保留
- [ ] 不停车连续循迹行为不变
- [ ] 终点停车正确

## Blocked by

- [06d-task-3-migration] — 最后迁移，需要前面所有 Task 验证通过
