# 03 — 声光指示模块

**Status:** ready-for-human

## Parent

`.docs/Analysis/底层重构实施计划_H0_fusion_training.md` — Slice D

## What to build

新建 `Drive/reach_point.c/h`，从 training 移植声光指示功能。小车到达关键节点时，通过 GPIO 输出 200ms 低电平脉冲驱动 LED + 蜂鸣器，提供外显反馈。

当前 H0 无此功能——比赛现场裁判无法确认节点到达，只能靠 OLED 读取。training 在所有任务的关键状态转换处都调用了 `point_OK()`。

**行为**：
- `ReachPoint_Trigger(0)` → PE14/PE15 输出低电平（点亮 LED/鸣响蜂鸣器）
- 200ms 后由 TIM6 tick 计数自动恢复到高电平（熄灭/静音）
- task.c 各任务在 count 变化或关键状态转换时调用

参考实现：`training/hardware/ReachPiont.c:3-7` — 仅 4 行业务逻辑。

**改动链路**：新建 reach_point.c 硬件驱动 → task.c 各任务插入触发 → main.c TIM6 ISR 管理自动恢复，一条完整的垂直切片。

## Acceptance criteria

- [ ] 小车经过节点时 LED/蜂鸣器短暂亮起 ~200ms
- [ ] 任务完成停车时持续亮起（或特殊模式指示）
- [ ] 不阻塞控制循环（200ms 由 ISR tick 计数管理，非 `HAL_Delay`）
- [ ] GPIO 引脚确认未被占用（PE14/PE15 当前未出现在 H0 引脚定义中）

## Blocked by

None — 可立即开始（但需先确认硬件 GPIO 空闲）
