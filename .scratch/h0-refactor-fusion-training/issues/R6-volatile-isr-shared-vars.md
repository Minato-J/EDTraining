# R6 — 补充 ISR 共享变量的 `volatile` 修饰符

**Status:** done
**Severity:** 🟡 P1（line_sensor_data）+ 🟡 P2（ctrl_mode）+ 🔵 P2（turn_out_smooth）

## Parent

Issue 06 — ctrl_mode 显式控制模式大重构
P0 修复 (commit `0a85fca`) — ISR 共享变量 volatile + 角度过滤器 float 哨兵改布尔

## What to build

重构引入了 3 个 ISR 与主循环共享但缺少 `volatile` 的变量。根据 C 标准，ISR 与主循环共享的变量必须声明 `volatile` 以防止编译器将值缓存在寄存器中。

### 1. `line_sensor_data`（P1）— `Drive/k230_track.c` + `Drive/k230_track.h`

- **写**：USART3 中断 `K230_Parse_Byte()`
- **读**：TIM6 中断 `Car_ControlLoop()` + 主循环 DEBUG_UART
- **现状**：`uint8_t line_sensor_data = 0x00;`（无 volatile）
- **对比**：同文件下一行 `volatile uint8_t k230_data_valid = 0;` 已正确声明 — 属于 Issue 02 引入但遗漏了 `line_sensor_data`
- **修复**：`.c` 定义和 `.h` extern 声明均加 `volatile`

### 2. `ctrl_mode`（P2）— `Drive/car_control.c`

- **写**：主循环 `Task_Dispatcher` → `Car_Stop()`（以及 `Car_StartLine`/`Car_StartStraight`/`Car_TurnTo` 当它们被调用时）
- **读**：TIM6 中断 `Car_ControlLoop()`
- **现状**：`static ctrl_mode_t ctrl_mode = CTRL_PARK;`（无 volatile）
- **对比**：同为 static + ISR 共享的 `blind_ticks`（line 26）已正确声明 `volatile`
- **修复**：加 `volatile`

### 3. `turn_out_smooth`（P2）— `Drive/car_control.c`

- **写**：TIM6 中断 `Car_ControlLoop()`
- **读**：主循环 `Car_GetTurnOutSmooth()`（DEBUG_UART）+ `Car_StartLine()`/`Car_ResetState()`
- **现状**：`static float turn_out_smooth = 0.0f;`（无 volatile）
- **修复**：加 `volatile`

## Acceptance criteria

- [ ] `line_sensor_data` 定义和声明均加 `volatile`
- [ ] `ctrl_mode` 加 `volatile`
- [ ] `turn_out_smooth` 加 `volatile`
- [ ] 编译零错误零警告

## Blocked by

None - can start immediately（与 R5 正交，可并行开发）
