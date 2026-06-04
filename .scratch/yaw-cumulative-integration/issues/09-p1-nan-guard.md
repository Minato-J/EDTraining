# 09 — P1: NaN 全链路防护

**Status:** ready-for-agent

## Parent

Code Review: [yaw-cumulative-integration CHANGELOG](../CHANGELOG.md) — P1#2

## What to build

IEEE 754 NaN 的关键属性：`NaN > X`、`NaN < X`、`NaN == X` 全部返回 false。这导致控制链上所有守卫条件对 NaN 完全透明。

**传播链**（一旦进入，唯一恢复手段是硬件复位）:
```
raw_yaw=NaN → fabsf(NaN)>30=false（绕过过滤器）
→ current_angle=NaN → YawTrack_Update: yaw_cumulative 永久 NaN
→ PID_Compute: integral+NaN=NaN（PID 永久失能）
→ (int16_t)NaN 未定义行为（ARMCC 产生 0 或 -32768）
→ 电机暴冲或锁死
```

**威胁入口**: UART 电磁干扰（JY901S 弱校验帧）、RAM 位翻转（电机 EMI）、栈溢出。

修复方案（纵深防御，两道防线）：

**第一道 — raw_yaw 入口哨兵**（main.c，raw_yaw 计算后立即检查）:
```c
float raw_yaw = IMU_Data.Yaw - Yaw_Offset;
if (isnan(raw_yaw)) {
    // 丢弃本帧所有控制计算，保持上次输出
    return;  // 或 continue（视上下文）
}
```

**第二道 — PID 输入前检查**（PID_Compute 内部或调用前）:
```c
if (isnan(target) || isnan(measured)) {
    return pid->output;  // 保持上次输出，不更新状态
}
```

Cortex-M4F 有硬件单精度 FPU，`isnan()` 对 `float` 直接编译为 `VCMP` + `VMRS` 两条指令，开销 ~2 周期，可忽略。

需要 `<math.h>`（或直接用 `(x != x)` 这个 IEEE 754 恒真 NaN 检测惯用法，无需头文件）。

修改范围：
- `Core/Src/main.c` — raw_yaw 计算后加 NaN 哨兵
- `Drive/pid.c` — `PID_Compute()` 入口加 NaN 检查

## Acceptance criteria

- [ ] raw_yaw 计算后加入 NaN 检查，NaN 时跳过本帧控制更新
- [ ] `PID_Compute()` 入口对 target/measured 做 NaN 检查，NaN 时返回上次 output 不更新状态
- [ ] NaN 检测使用 `x != x` 惯用法（无额外头文件依赖），或包含 `<math.h>` 用 `isnanf()`
- [ ] 验证：人工注入 NaN（调试器修改寄存器）→ 控制输出不变，系统不自愈但也不暴冲
- [ ] Keil MDK 编译 0 错误 0 警告

## Blocked by

无 — 可立即开始
