# 04 — UART1 调试输出恢复

**Status:** ready-for-agent

## Parent

`.docs/Analysis/底层重构实施计划_H0_fusion_training.md` — Slice E

## What to build

恢复 H0 中被注释掉的 UART1 调试输出，以可配置周期发送编码器 + 角度 + 传感器数据到串口助手。

当前 H0 的 printf 被注释掉，现场调试只能靠 OLED（4 行显示，信息量有限）。training 有完整的 `usart_transmit()` 实现可以参考。

**格式**：
```
"%d,%d,%.1f,%.1f,%d,%.1f\n\r"
(Encoder_L, Encoder_R, Yaw, target_angle, line_sensor_data, turn_out)
```

**发送策略**：
- 由 TIM6 ISR 中设置 `flag_usart1` 标志触发（参考 training 方案）
- 每 5 个 TIM6 tick（100ms）发送一次，不在 ISR 内直接发送
- while(1) 主循环中检测标志 → 调用 `HAL_UART_Transmit` 发送
- `#ifdef DEBUG_UART` 条件编译，未定义时零开销

参考实现：`training/hardware/myusart.c:91-100` usart_transmit()

**改动链路**：TIM6 ISR 置标志 → while(1) 检测标志 → UART1 格式化输出，一条完整的垂直切片。改动仅限 main.c，影响最小。

## Acceptance criteria

- [ ] 串口助手能实时接收编码器 + 角度 + 传感器数据
- [ ] `DEBUG_UART` 宏未定义时编译产物与改造前完全一致（零开销）
- [ ] 不影响 TIM6 中断实时性（发送操作在 ISR 外的主循环中执行）
- [ ] 发送周期可通过宏调整（默认 5 ticks = 100ms）

## Blocked by

None — 可立即开始
