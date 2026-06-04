# 11 — P2: count 变量鲁棒性加固

**Status:** ready-for-agent

## Parent

Code Review: [yaw-cumulative-integration CHANGELOG](../CHANGELOG.md) — P2#6 + P2#7

## What to build

两个与 `count` 变量相关的低概率但真实存在的缺陷：

### 问题 1: count++ 与 count_debounce 间的指令级竞争

主循环中 `count++; count_debounce = DEBOUNCE_INIT;` 两条 C 语句之间有 ~3-5 条汇编指令（~30ns @168MHz）。若 TIM6 ISR 恰好在此窗口中触发且检测到边沿，ISR 执行第二次 `count++`，count 一次跳 2，跳过整条路段。

**概率**: ~30ns / 20ms = 1.5×10⁻⁶（单次 YawTrack 事件），但对竞赛而言不可接受。

### 问题 2: uint8_t count 溢出

`uint8_t count` 最大 255。Task 4 中若节点边缘反复振荡（ISR 每 200ms 递增一次），持续 51 秒 → count 从 255 翻转到 0 → `count >= N` 永久失效。

修复方案：

**问题 1**: 用 `__disable_irq()/__enable_irq()` 包裹 YawTrack 兜底路径的 `count++` + `count_debounce = DEBOUNCE_INIT`（task.c 中约 3 处）。ISR 路径（main.c）不需要改——中断内部已天然原子。

**问题 2**: 将 `count` 类型从 `uint8_t` 改为 `uint16_t`（最大 65535，Task 4 需 200ms×65535≈3.6 小时才溢出）。

修改范围：
- `Drive/task.h` — `extern uint8_t count` → `extern uint16_t count`
- `Drive/task.c` — 定义 + 3 处 YawTrack 临界区包裹
- `Core/Src/main.c` — `count++` 处类型匹配
- `Core/Inc/main.h` — 如有 extern 也需同步

## Acceptance criteria

- [ ] `count` 类型从 `uint8_t` 改为 `uint16_t`，task.h 和 task.c 同步
- [ ] main.c ISR 中 `count++` 与 uint16_t 类型兼容
- [ ] task.c YawTrack 兜底路径 `count++` + `count_debounce` 用 `__disable_irq()/__enable_irq()` 保护（3 处）
- [ ] 临界区包裹后立即恢复中断，不在关中断状态下做其他操作
- [ ] Keil MDK 编译 0 错误 0 警告

## Blocked by

无 — 可立即开始

## 注意

- `__disable_irq()` 只屏蔽**可屏蔽中断**（不包括 NMI/HardFault），STM32F4 上 TIM6 在可屏蔽范围内，保护有效
- 如果用 CMSIS 风格，等效函数是 `__set_PRIMASK(1)` / `__set_PRIMASK(0)`
- 也可以考虑将 count 的**所有**修改路径统一到 ISR 中（通过标志位通知主循环），但改动更大，留给后续重构
