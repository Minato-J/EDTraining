# 02 — K230 数据有效标志 + 帧超时

**Status:** ready-for-agent

## Parent

`.docs/Analysis/底层重构实施计划_H0_fusion_training.md` — Slice B

## What to build

在 K230 串口解析模块中引入数据新鲜度机制，让 H0 能区分"刚收到的有效帧"和"已过期的旧数据"。

当前问题：H0 无 `k230_data_valid` 标志，`line_sensor_data` 只要被写过就一直被信任。如果 K230 物理断开，小车会抱着最后一帧有效数据跑循迹模式，而不是安全降级到盲开。

**机制**：
1. `K230_Parse_Byte()` 成功解析一帧（状态机走到 DONE）后置 `k230_data_valid = 1`
2. TIM6 中断每 tick 递增超时计数器；收到有效帧时清零
3. 超时阈值 5 ticks（~100ms @20ms 周期），超时后清零 `k230_data_valid`
4. main.c 双模切换中，`!k230_data_valid` 强制走盲开分支（即使 `line_sensor_data` 缓存了旧值）

参考：`training/hardware/k230.c` 在帧尾 `0xCC` 匹配后设 `k230_data_valid = 1`。H0 的 3 字节帧无帧尾，在 `K230_Parse_Byte` state==2（DONE）状态下等效处理。

**改动链路**：k230_track.c 解析层 → main.c TIM6 ISR 超时检测 → 双模切换决策层，一条完整的垂直切片。

## Acceptance criteria

- [ ] 拔掉 K230 串口线后，5 ticks（~100ms）内自动进入盲开模式
- [ ] 重新连接 K230 后，下一帧有效数据立即恢复循迹模式
- [ ] 正常运行时 valid 标志不影响现有行为（无频繁误切盲开）
- [ ] 超时阈值可通过宏调整

## Blocked by

None — 可立即开始
