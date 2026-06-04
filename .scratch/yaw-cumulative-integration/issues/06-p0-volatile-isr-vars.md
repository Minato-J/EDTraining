# 06 — P0: volatile 修饰 ISR 共享变量

**Status:** ready-for-agent

## Parent

Code Review: [yaw-cumulative-integration CHANGELOG](../CHANGELOG.md) — P0#1

## What to build

ARM Compiler 5 在 `-O2` 优化下有权将 ISR ↔ 主循环共享的全局变量提升到寄存器中。ISR 写入内存后，主循环读到的是寄存器中的过期副本，导致逻辑失效。

需要加 `volatile` 的三个变量：

- **`count`** — ISR (TIM6 `count++`) 与主循环 (YawTrack 兜底 `count++`) 双重递增
- **`count_debounce`** — ISR 和主循环同时写入去抖窗口值
- **`task_running`** — ISR 写入（盲开超时停车），主循环读取

**最坏场景验证**: 盲开超时 ISR 设 `task_running=0` → 主循环寄存器缓存为 1 → 电机已停但任务函数持续输出非零 PID 目标。

修改范围：
- `Drive/task.h` — extern 声明加 `volatile`
- `Drive/task.c` — 定义加 `volatile`
- `Core/Src/main.c` — 如有 extern 引用也需同步（通常通过 task.h）

## Acceptance criteria

- [ ] `task.h` 中 `count`、`count_debounce`、`task_running` 的 extern 声明加 `volatile`
- [ ] `task.c` 中对应定义加 `volatile`
- [ ] main.c 中若有这些变量的 extern 引用，与 task.h 一致
- [ ] Keil MDK 编译 0 错误 0 警告（ARMCC -O2）
- [ ] 代码风格与 yaw_track.c 中已有的 `static volatile float` 一致

## Blocked by

无 — 可立即开始
