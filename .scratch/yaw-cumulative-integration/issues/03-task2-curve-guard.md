# 03 — Task 2 外圈加 yaw_cumulative 二级校验

**Status:** ready-for-agent

## Parent

PRD: [yaw-cumulative-integration](../PRD.md)

## What to build

在 Task 2（A→B→C→D→A 外圈）的弯道段中加入 yaw_cumulative 兜底逻辑。

Task 2 有 2 个弯道段：
- `case 1`：B→C 右半圆弧（target_angle=-185°, base_speed=30, ff_diff=10）
- `case 3`：D→A 左半圆弧（target_angle=+185°, base_speed=30, ff_diff=-10）

修改方式：进入弧段 case 时先 `YawTrack_Reset()`，然后在 case 内部持续检测 `YawTrack_IsCurveDone(200.0f)`，一旦累计角度超过 200°（约半圆弧的理论航向变化），强制 `count++` 并跳出。

具体实现技巧：由于 `Run_Task_2` 是每 10ms 主循环调用的 switch(count)，可在 case 1 和 case 3 内部加 if 判断，当 `YawTrack_IsCurveDone(200.0f)` 为真时 `count++`（让下一次调用走下一个 case）。

注意：`Run_Task_2` 由 `Task_Dispatcher` 从 while(1) 主循环调用（~10ms 周期），而非从 TIM6 中断调用。yaw_cumulative 的更新在 TIM6 中断中（~20ms），两者无竞争——yaw_cumulative 是单调递增的 float，读写不需要加锁。

## Acceptance criteria

- [ ] B→C 弧段（case 1）：count 正常触发时流程不变；count 漏触发时 yaw_cumulative > 200° 兜底推进到 case 2
- [ ] D→A 弧段（case 3）：同上，兜底推进到 case 4
- [ ] 直线段（case 0, case 2）不受影响
- [ ] 不影响 Task 1 和 Task 3/4/5 的行为
- [ ] 编译 0 错误 0 警告

## Blocked by

- [02 — 接入 TIM6 中断 + 发车时 Reset](./02-integrate-tim6-and-start-reset.md)
