# 04 — Task 3/4/5 全部弯道段加 yaw_cumulative 兜底

**Status:** ready-for-agent

## Parent

PRD: [yaw-cumulative-integration](../PRD.md)

## What to build

在 Task 3、Task 4、Task 5 的所有弯道段中加入 `YawTrack_Reset()` + `YawTrack_IsCurveDone()` 二级校验。

### Task 3（count 驱动 + 进弯刹车）

3 个弯道段：
- `case 1`：C→B 弧段 — 进入时已有 `t3_wait_tick` 进弯刹车，加 `YawTrack_Reset()` 在 `t3_last_count != count` 分支（即进 case 的首次调用），然后检测 `YawTrack_IsCurveDone(180.0f)` → `count = 2`
- `case 2`：B→D 对角弧段 — 同上，阈值 180.0f → `count = 3`
- `case 3`：D→A 弧段 — 同上，阈值 180.0f → `count = 4`

### Task 4（航点状态机，29 状态）

Task 4 不是 switch(count) 而是 switch(current_state)，弯道段的判定依赖 `count >= N`。在等待 count 的"保持推进"状态中加入 yaw_cumulative 兜底：

需要兜底的"保持推进"状态（每圈 4 个 × 3 圈 = 12 个）：
- `case 1/3/5/7`（第一圈）：`if (count >= N)` → 改为 `if (count >= N || YawTrack_IsCurveDone(180.0f))`
- `case 11/13/15/17`（第二圈）：同上
- `case 21/23/25/27`（第三圈）：同上

每进入新的弧段推进状态时（状态 0→1, 2→3, 4→5, 6→7 等），在前面设置 target_angle 的状态中调用 `YawTrack_Reset()`。

### Task 5（count 驱动 4 圈）

2 个弯道段：
- `count == 1`（右弧段）：`YawTrack_Reset()` 在首次进入时调用，`YawTrack_IsCurveDone(200.0f)` → `count = 2`
- `count == 3`（左弧段）：同上 → `count = 4`（触发 lap_count 逻辑）

由于 Task 5 的 if-else 每次循环都执行，需要加 static 变量标记是否已在本段调用过 Reset，避免每 10ms 重复 Reset。

## Acceptance criteria

- [ ] Task 3 三个弯道段各可被 yaw_cumulative 兜底推进
- [ ] Task 4 三圈共 12 个推进状态各可被兜底推进，不影响正常的 count 触发路径
- [ ] Task 5 两个弯道段各可被兜底推进，多圈 Reset 不重复
- [ ] 进弯刹车机制（Task 3）与 yaw_cumulative 不冲突
- [ ] 编译 0 错误 0 警告

## Blocked by

- [03 — Task 2 外圈加 yaw_cumulative 二级校验](./03-task2-curve-guard.md)
