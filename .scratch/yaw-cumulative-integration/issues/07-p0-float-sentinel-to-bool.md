# 07 — P0: 角度过滤器 float 哨兵改为布尔标志

**Status:** ready-for-agent

## Parent

Code Review: [yaw-cumulative-integration CHANGELOG](../CHANGELOG.md) — P0#2

## What to build

`Core/Src/main.c:263` 行角度突变过滤器使用 `last_valid_angle != 0` 作为"是否已有首个有效读数"的哨兵。但 `target_angle=0` 是 Task 1/2 的正常稳态航向——当小车航向恰好经过 0° 时，`last_valid_angle` 被更新为 `0.0f`，哨兵失效。

**触发链**: 0° 稳态航向 → IMU 跳变 100° → `fabsf(diff)>30` 为真，`last_valid_angle!=0` 为假 → `if` 整体为 false → 尖峰绕过过滤器 → `current_angle` 被污染 → 差速合成输出错误值 → 小车剧烈扭动。

修复方案：用独立的 `static uint8_t angle_initialized` 布尔标志替代 float 哨兵。

修改范围：
- `Core/Src/main.c` 约 263-268 行，仅改角度过滤器代码块

伪码：
```c
static float last_valid_angle = 0;
static uint8_t angle_initialized = 0;   // 新增

float raw_yaw = IMU_Data.Yaw - Yaw_Offset;
float diff = raw_yaw - last_valid_angle;
while (diff > 180.0f)  diff -= 360.0f;
while (diff < -180.0f) diff += 360.0f;

if (!angle_initialized) {
    // 首个有效读数：无条件接受
    current_angle = raw_yaw;
    last_valid_angle = raw_yaw;
    angle_initialized = 1;
} else if (fabsf(diff) > 30.0f) {
    // 突变：丢弃，保持上次有效值
    current_angle = last_valid_angle;
} else {
    // 正常：接受并更新
    current_angle = raw_yaw;
    last_valid_angle = raw_yaw;
}
```

## Acceptance criteria

- [ ] 新增 `static uint8_t angle_initialized` 标志
- [ ] 重构 if-else 为三路分支（首帧 / 突变丢弃 / 正常更新）
- [ ] `angle_initialized` 在任务启动时（Task_Key_Scan Start 处理）归零，或改用非 static 变量由外部控制
- [ ] 验证：target_angle=0° 稳态时 IMU 跳变不再绕过过滤器
- [ ] Keil MDK 编译 0 错误 0 警告

## Blocked by

无 — 可立即开始
