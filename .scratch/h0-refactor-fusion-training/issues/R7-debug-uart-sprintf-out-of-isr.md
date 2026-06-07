# R7 — 将 DEBUG_UART `sprintf` 移出 ISR

**Status:** wontfix
**Severity:** 🔵 P3 (REFUTED — 误报)

## Refutation

`sprintf` + `HAL_UART_Transmit` 实际位于 `main.c` 的 `while(1)` 主循环中（line 188-198），不在 TIM6 ISR 中。ISR 中仅有 `debug_tick_cnt++`（计数器递增，轻量操作）。无需修改。

## Parent

Issue 04 — UART1 调试输出

## What to build

当前 DEBUG_UART 实现在 TIM6 ISR 中每 5 tick (100ms) 执行 `sprintf(debug_buf, ...)` + `HAL_UART_Transmit()`。`sprintf` 计算量大且标准库实现通常不可重入，在 ISR 上下文中执行可能导致：

- ISR 执行时间抖动（影响 20ms 控制周期精度）
- 栈溢出风险（`sprintf` 内部栈消耗大）
- 若被更高优先级中断抢占且该中断也调用 `sprintf` 系列函数，数据损坏

### 修复方案

ISR 中仅设置 flag 并将数据拷贝到环形缓冲区，主循环中执行实际的 `sprintf` + `HAL_UART_Transmit`。

具体实现：
1. 在 `main.c` 中新增一个 `debug_data_ready` 标志（volatile）和一个小的数据快照结构体
2. ISR 中：`debug_tick_cnt` 达到阈值时，拷贝 `current_v_left/right`、`current_angle`、`target_angle`、`line_sensor_data`、`Car_GetTurnOutSmooth()` 到快照，设 `debug_data_ready = 1`
3. 主循环 `while(1)` 中：检查 `debug_data_ready`，若为真则 `sprintf` + `HAL_UART_Transmit`，清零标志

此修改仅在 `#ifdef DEBUG_UART` 块内，不影响正式固件。

## Acceptance criteria

- [ ] `sprintf` 和 `HAL_UART_Transmit` 移出 ISR，仅在主循环执行
- [ ] ISR 中仅拷贝数据（无重量级函数调用）
- [ ] 调试输出功能等价（每 100ms 输出一行）
- [ ] `#ifdef DEBUG_UART` 关闭时零开销
- [ ] 编译零错误零警告

## Blocked by

None - can start immediately（最低优先级，与其他 issue 无依赖）
