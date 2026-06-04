# 01 — 新建 yaw_track 模块 + 核心算法

**Status:** ready-for-agent

## Parent

PRD: [yaw-cumulative-integration](../PRD.md)

## What to build

新建 `Drive/yaw_track.c` 和 `Drive/yaw_track.h`，实现 4 个 API：`YawTrack_Reset`、`YawTrack_Update`、`YawTrack_GetCumulative`、`YawTrack_IsCurveDone`。

核心算法：
- `YawTrack_Update(float current_yaw)` 每帧计算 `delta = current_yaw - yaw_prev`，处理 ±180° 跳变后累加到 `yaw_cumulative`
- 累加使用 `fabs(delta)` 取绝对值，不区分左右弯
- `YawTrack_Reset()` 将累计值清零，`track_enable` 置 1，`yaw_prev` 记录当前角度
- `YawTrack_GetCumulative()` 直接返回 `yaw_cumulative`
- `YawTrack_IsCurveDone(float threshold_deg)` 返回 `yaw_cumulative > threshold_deg`

内部变量（static 文件作用域）：
- `float yaw_cumulative` — 累计航向变化（度）
- `float yaw_prev` — 上一帧 yaw 值
- `uint8_t track_enable` — 使能标志

本 issue 只创建模块文件，暂不接入任何调用方。将 yaw_track.c 加入 Keil 工程编译。

## Acceptance criteria

- [ ] `Drive/yaw_track.h` 声明 4 个 API，包含 `#ifndef` 头文件保护
- [ ] `Drive/yaw_track.c` 实现 4 个函数，核心逻辑正确
- [ ] ±180° 跳变正确处理（验证：yaw_prev=179°, current_yaw=-179° → delta=2° 而非 358°）
- [ ] 文件已加入 Keil MDK 工程，编译 0 错误 0 警告
- [ ] 代码风格与现有 `Drive/` 层一致（命名、注释密度、缩进）

## Blocked by

无 — 可立即开始
