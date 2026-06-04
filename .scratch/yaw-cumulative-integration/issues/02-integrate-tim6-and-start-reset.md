# 02 — 接入 TIM6 中断 + 发车时 Reset

**Status:** ready-for-agent

## Parent

PRD: [yaw-cumulative-integration](../PRD.md)

## What to build

将 yaw_track 模块接入 H0 的控制循环：

1. **main.c**：在 TIM6 中断回调中，角度突变过滤器得出 `current_angle` 之后，加入 `YawTrack_Update(current_angle)` 调用（1 行）
2. **task.c**：在 `Task_Key_Scan()` 的 Start 按键处理中（`task_running = 1` 之前），调用 `YawTrack_Reset()` 清零累计值
3. **OLED 调试**：在 OLED 显示区新增一行显示 `yaw_cumulative` 值，便于现场验证

改动文件：
- `Core/Src/main.c`：`#include "yaw_track.h"` + TIM6 中断中 1 行调用 + OLED 显示行
- `Drive/task.c`：`#include "yaw_track.h"` + 发车时 1 行 `YawTrack_Reset()`

## Acceptance criteria

- [ ] TIM6 每 20ms 周期调用 `YawTrack_Update(current_angle)`，yaw_cumulative 正确累加
- [ ] 按 Start 键后 yaw_cumulative 从 0 开始，不会保留上次任务的残留值
- [ ] OLED 可实时显示累计角度值（验证工具，后续可移除或保留）
- [ ] 编译 0 错误 0 警告
- [ ] 静态验证：手动转动小车，OLED 上累计值持续增长

## Blocked by

- [01 — 新建 yaw_track 模块 + 核心算法](./01-new-yaw-track-module.md)
