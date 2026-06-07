# R4 — Task 4 推进状态补 Car_SetFFDiff(0)

**Status:** ready-for-agent
**Severity:** 🔵 P2 (预存问题)

## Parent

Issue 06e — Task 4 迁移

## What to build

Task 4 部分推进状态（case 11, 13, 15, 17, 21, 23, 25, 27）在旧代码中就从未设置 `ff_diff = 0`。这些状态只设了 `base_speed` 和 `target_angle`，如果前一个路段残留了非零 ff_diff（如弧段的 ±10），差速偏置会持续污染后续直线/对角线路段。

重构用 sed 批量替换时忠实地保留了原行为（只替换了已存在的 `ff_diff = 0` 行），但既然现在用 API 了，应显式清零消除隐式状态依赖。

修复：在 8 个缺 `Car_SetFFDiff(0)` 的 case 中补上。

涉及 case：第二圈 11, 13, 15, 17 + 第三圈 21, 23, 25, 27

## Acceptance criteria

- [ ] 8 个 case 均显式调用 Car_SetFFDiff(0)
- [ ] 无隐式状态跨路段污染
- [ ] 编译零错误零警告

## Blocked by

None - can start immediately
