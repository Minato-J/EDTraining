# 08 — P1+P2: ISR 静态状态任务启动时批量清零

**Status:** ready-for-agent

## Parent

Code Review: [yaw-cumulative-integration CHANGELOG](../CHANGELOG.md) — P1#3 + P2#5 + P2#8

## What to build

三个 TIM6 ISR 内部的 `static` 变量在任务正常结束时未归零，下次 Start 时从残留值继续运行：

| 变量 | 位置 | 残留后果 |
|------|------|---------|
| `blind_ticks` | main.c:275 | 残留 49 → Start 后首个丢线帧即触发紧急停车 (1 tick) |
| `turn_out_smooth` | main.c:313 | 残留旧转弯量 → 新任务前 60-100ms 输出错误差速 |
| PID integral/last_error/output | pid.c | 前次积分残留 → 新任务前几帧控制瞬变 |

修复方案：

1. **`blind_ticks`** 和 **`turn_out_smooth`**：从 ISR 内部 static 提升为文件级变量（main.c 顶部），在 `Task_Key_Scan` Start 处理中显式归零。需要在 task.c 中通过 extern 访问，或提供一个 `Reset_ControlState()` 函数。

2. **PID 状态**：在 `Task_Key_Scan` Start 处理中，对 `pid_left`、`pid_right`、`pid_angle` 三个 PID 实例调用 `PID_Init()` 重新初始化。

推荐实现：在 main.c 中新增 `ControlState_Reset(void)` 函数，集中清零 `blind_ticks`、`turn_out_smooth`，并调用三次 `PID_Init`。task.c 的 Start 处理中调用此函数。

修改范围：
- `Core/Src/main.c` — 新增 `ControlState_Reset()`，含 `extern PID_TypeDef pid_left, pid_right, pid_angle`
- `Core/Inc/main.h` — 声明 `ControlState_Reset()`
- `Drive/task.c` — Start 处理中调用 `ControlState_Reset()`

## Acceptance criteria

- [ ] `blind_ticks` 从 static 局部变量提升为文件级变量
- [ ] `turn_out_smooth` 从 static 局部变量提升为文件级变量
- [ ] 新增 `ControlState_Reset()` 函数，清零 blind_ticks、turn_out_smooth，调用三次 PID_Init
- [ ] `Task_Key_Scan` Start 处理中调用 `ControlState_Reset()`
- [ ] 验证：两次连续任务启动间 PID 状态无残留
- [ ] Keil MDK 编译 0 错误 0 警告

## Blocked by

无 — 可立即开始
