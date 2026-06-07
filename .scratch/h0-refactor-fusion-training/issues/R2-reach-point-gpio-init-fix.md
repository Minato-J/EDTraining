# R2 — reach_point.c PE14/PE15 缺 GPIO 输出模式初始化

**Status:** ready-for-agent
**Severity:** 🟡 P1

## Parent

Issue 03 — 声光指示模块

## What to build

`ReachPoint_Init()` 当前只调用 `HAL_GPIO_WritePin()` 设置输出电平，但从未通过 `HAL_GPIO_Init()` 将 PE14/PE15 配置为 GPIO 输出模式。STM32F4 复位后 MODER 默认为输入(00)，输出驱动器禁用。`MX_GPIO_Init()` (CubeMX 生成) 也只配置了 PE2~PE6，不含 PE14/PE15。

修复：在 `ReachPoint_Init()` 开头增加 `GPIO_InitTypeDef` 配置，将 PE14/PE15 初始化为推挽输出模式。

## Acceptance criteria

- [ ] PE14/PE15 通过 HAL_GPIO_Init 配置为 GPIO_MODE_OUTPUT_PP
- [ ] 初始化后默认高电平（熄灭/静音）
- [ ] 编译零错误零警告

## Blocked by

None - can start immediately
