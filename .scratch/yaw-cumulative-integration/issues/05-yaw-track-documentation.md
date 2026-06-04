# 05 — yaw_track.c 头部实现逻辑文档 + 详细注释

**Status:** ready-for-agent

## Parent

PRD: [yaw-cumulative-integration](../PRD.md)

## What to build

在 `Drive/yaw_track.c` 文件头部添加模块级别的实现逻辑文档，并对核心代码段添加行内注释。

### 头部文档要求

用多行注释块（`/* */`）写在 `#include` 之前，覆盖以下内容：

1. **模块目的**：1~2 句说明 yaw_cumulative 是什么、解决什么问题
2. **核心算法**：描述 `delta = current - prev` → 跳变处理 → `fabs(delta)` 累加的流程
3. **±180° 跳变处理**：解释为什么需要这个处理（例如从 +179°→-179°，实际只转了 2°，但 naive 减法会得到 -358°），以及代码如何修正（while 循环归化到 (-180, +180]）
4. **调用时机**：列出谁在什么时候调用哪个 API
   - `YawTrack_Reset()` — task.c 发车时 + 进入弯道段时
   - `YawTrack_Update()` — main.c TIM6 中断，每 20ms
   - `YawTrack_GetCumulative()` / `YawTrack_IsCurveDone()` — task.c 各任务弯道 case
5. **设计决策**：为什么用 `fabs` 取绝对值而非保留符号（简化阈值判断，不区分左弯/右弯）；为什么不在直道段累计（避免噪声积累）

### 行内注释要求

- `YawTrack_Update` 的 delta 计算和跳变处理各加 1 行注释
- `YawTrack_Reset` 加注释说明"发车时或进入新弯道段时调用"
- 每个 static 变量加注释说明用途

### 不做的事

- 不改动 .h 文件
- 不改动任何算法逻辑
- 不改动其他文件

## Acceptance criteria

- [ ] yaw_track.c 头部有完整的多行注释块，覆盖上述 5 点
- [ ] 核心代码行有简洁的行内注释
- [ ] 注释使用中文（与项目其他文件一致）
- [ ] 编译 0 错误 0 警告（注释不影响编译）

## Blocked by

- [01](./01-new-yaw-track-module.md)
- [02](./02-integrate-tim6-and-start-reset.md)
- [03](./03-task2-curve-guard.md)
- [04](./04-task345-curve-guard.md)
