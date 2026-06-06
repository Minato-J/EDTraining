# 05 — K230 加权平均法转弯量计算

**Status:** ready-for-agent

## Parent

`.docs/Analysis/底层重构实施计划_H0_fusion_training.md` — Slice C

## What to build

在 K230 转弯量计算中新增**加权平均法**作为编译期可选方案，替代当前离散 13 档查表法。

当前 H0 的 `K230_Get_Turn_Speed()` 使用 switch-case 查表法，将 64 种灰度组合映射到 13 档离散转弯量（±2~±20）。加权平均法通过 6 路权重计算连续转弯量，分辨率为理论 N² 档。

**算法**（参考 `training/hardware/CarDrive.c:36-49` calc_line_error）：

```c
static const int sensor_weight[6] = {-5, -3, -1, 1, 3, 5};

// 对 6 路灰度逐位检测，累加权重 → 除以检测到的线数 → 乘以缩放系数
// 0x00（全白）返回上次有效值
// count==0 时返回 0（无黑线→不修正）
```

**设计决策**：
- 编译期宏 `K230_USE_WEIGHTED` 切换查表法 / 加权平均法
- 保留查表法作为已验证的 fallback
- training 是 7 路传感器（权重 `{-3,-2,-1,0,1,2,3}`），H0 是 6 路——权重改为非对称映射，无中间零路
- 加权平均连续值需经过与查表法相同的 `turn_out_smooth` 低通滤波

**改动链路**：k230_track.c 算法层 → main.c TIM6 ISR 调用层 → turn_out 滤波 → 差速合成 → 电机，一条完整的垂直切片。

## Acceptance criteria

- [ ] 循迹转弯量连续变化，不再有阶梯跳跃
- [ ] 0x00 全白时返回 last_valid_turn（配合 Slice 02 的 valid 标志降级到盲开）
- [ ] K230 任意单路灰度损坏时，加权平均优雅退化（count 减少但 sum/count 仍有效）
- [ ] `K230_USE_WEIGHTED` 宏未定义时编译行为与改造前完全一致
- [ ] 桌面测试向量验证 6 路权重映射正确

## Blocked by

- [02-k230-data-valid-timeout] — 需要 valid 标志区分"丢线（K230 断开）"和"全白（在黑线上但传感器全灭）"
