# R3 — Car_ControlLoop 盲开超时停车后 target_v 残留一帧修复

**Status:** ready-for-agent
**Severity:** 🟡 P2

## Parent

Issue 06a — car_control 模块

## What to build

`Car_ControlLoop()` CTRL_LINE 分支中盲开超时触发紧急停车时：

```c
if (blind_ticks > 50) {
    Car_Stop();          // 设 ctrl_mode=CTRL_PARK，target_v 未动
    task_running = 0;
    return;              // 提前 return，跳过公共后处理
}
```

旧代码中盲开超时后继续执行差速合成（此时 base_speed=0，turn_out≈0），target_v 接近零。新代码 `return` 令 target_v_left/right 保留上一 tick 的行驶值，延迟一帧（20ms）才被下一 tick 的 CTRL_PARK 清零。

修复：在 `return` 前补 `target_v_left = 0; target_v_right = 0;`。

## Acceptance criteria

- [ ] 盲开超时触发时 target_v 立即归零（不等下一帧）
- [ ] 行为等价于旧代码
- [ ] 编译零错误零警告

## Blocked by

None - can start immediately
