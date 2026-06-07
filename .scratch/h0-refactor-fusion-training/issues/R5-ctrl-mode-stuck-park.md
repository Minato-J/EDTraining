# R5 — 修复 `ctrl_mode` 永远停留在 CTRL_PARK（全部 5 个 Task 瘫痪）

**Status:** done
**Severity:** 🔴 P0

## Parent

R1 — Task 1/2/3/5 Car_StartLine 每 tick 清零 turn_out_smooth 修复

## What to build

R1 修复将 `Car_StartLine()` 替换为独立 setter（`Car_SetTargetAngle` / `Car_SetSpeed` / `Car_SetFFDiff`）后，再无代码将 `ctrl_mode` 从 `CTRL_PARK` 切换到行驶模式。`Car_ControlLoop()` 入口检查 `ctrl_mode == CTRL_PARK` 直接返回 `target_v=0`，5 个 Task 全部瘫痪。

**核心问题**：独立 setter 不修改 `ctrl_mode`，而修改 `ctrl_mode` 的三个函数（`Car_StartLine` → CTRL_LINE、`Car_StartStraight` → CTRL_STRAIGHT、`Car_TurnTo` → CTRL_TURN）从未被 task.c 调用。

### 修复策略（按 Task 类型区分）

**count 驱动型 Task（1/2/3/5）**：利用已有的 `tX_last_count` 检测 `count` 变化。在 count 变化时（首次进入新路段）调用 `Car_StartLine()` 一次性完成模式切换 + 参数设置；后续同一 count 的 tick 中不重复调用（或改用独立 setter 保持 turn_out_smooth 不重置）。

Task 3 的刹车 case（1/2/3）特殊处理：首次进入时 `Car_StartLine(angle, speed, 0)` 设模式，后续 tick 用 `Car_SetSpeed()` 调整刹车速度但不重置平滑状态。

**状态机型 Task（4）**：分为两类状态：
- **设置态**（setup states）：0/2/4/6/10/12/14/16/20/22/24/26 — 一次性进入即 `break`，调用 `Car_StartLine()` 切换模式
- **推进态**（propulsion states）：1/3/5/7/11/13/15/17/21/23/25/27 — 每 tick 循环执行，保持独立 setter 不改 turn_out_smooth

或者，更简洁的方案：`Car_StartLine()` 增加一个 `keep_smooth` 参数，当为 true 时不重置 `turn_out_smooth`。这避免了 count 驱动的每 tick 重复调用问题，同时保留了模式切换语义。但此方案需修改 API 签名。

## Acceptance criteria

- [ ] 按下 Start 后 `ctrl_mode` 正确切换到 `CTRL_LINE`（或对应行驶模式）
- [ ] Task 1 A→B 直线可行驶
- [ ] Task 2 外圈可行驶
- [ ] Task 3 对角线 1 圈可行驶
- [ ] Task 4 3 圈可行驶
- [ ] Task 5 4 圈可行驶
- [ ] `turn_out_smooth` 低通滤波不被每 tick 清零（R1 修复效果保持）
- [ ] 编译零错误零警告

## Blocked by

None - can start immediately
