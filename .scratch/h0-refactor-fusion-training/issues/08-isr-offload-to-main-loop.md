# Issue 08: TIM6 ISR 执行时间优化 — 拆分"重中断"为 ISR + 主循环协作

## Parent

无 — 架构审查发现

## What

当前 TIM6 ISR (`HAL_TIM_PeriodElapsedCallback`, ~20ms) 在中断上下文中执行了约 80 行 C 代码，包含：

1. 节点计数去抖（`count_debounce` 递减 + 边沿检测）
2. 角度突变过滤器（NaN 哨兵 + ±180° 归化 + 30° 阈值）
3. `YawTrack_Update()`（delta 计算 + fabsf 累加）
4. `Car_ControlLoop()`（ctrl_mode 分发 → K230 加权求均 / 角度环 PID / 平滑滤波 / 差速合成）
5. 左右轮速度环 PID ×2（增量式计算 + 编码器低通滤波）
6. `Motor_SetSpeed_A/B()`（PWM 寄存器写入）

**风险**：若 ISR 执行时间接近或超过 20ms，会导致：
- 中断重入被阻塞（Cortex-M4 同优先级不可嵌套）
- 主循环 `Task_Dispatcher()` 饥饿 → `count` 变化时 task 函数来不及切换模式
- 严重时触发 HardFault（中断栈溢出）

### 拆分方案：ISR 轻量化 + 主循环消费

```
TIM6 ISR (只做采集，<2ms):
  ├── 编码器原始值读取 (Encoder_Get_Count)
  ├── count_debounce / 边沿检测 → count++
  ├── 角度突变过滤器 → current_angle
  ├── YawTrack_Update(current_angle)
  ├── 组装 Car_SensorInputs in
  └── 设置 flag: control_data_ready = 1

main() while(1) 新增 (每循环检测 flag):
  if (control_data_ready) {
      Car_ControlOutputs out = Car_ControlLoop(in);
      → 速度环 PID ×2
      → Motor_SetSpeed
      control_data_ready = 0;
  }
```

### 关键决策点

| 决策 | 选项 A（推荐） | 选项 B |
|------|---------------|--------|
| 速度环放哪 | 主循环（ISR 只读数） | ISR（保持实时性） |
| 同步机制 | `volatile uint8_t flag` + `Car_SensorInputs` 快照 | 关中断临界区复制 |

选项 A 的理由：速度环已经有编码器低通滤波（0.7*old+0.3*new），延迟一帧影响极小。角度环同理——输出是转弯量/差速，半帧（~10ms）延迟在 20ms 周期中不可感知。

### 涉及的代码位置

- `Core/Src/main.c`: `HAL_TIM_PeriodElapsedCallback()` (~80行) + `main()` while(1)
- `Drive/car_control.c`: `Car_ControlLoop()` 本身不变，只是调用位置变了

## Acceptance criteria

- [ ] 编译 0 errors 0 warnings
- [ ] TIM6 ISR 执行时间从 ~80 行缩减到 ~30 行（只读数+设置 flag）
- [ ] 速度环 PID 和 Motor_SetSpeed 移到主循环
- [ ] 控制频率仍为 ~20ms（主循环 `HAL_Delay(10)` 改为 flag 等待）
- [ ] 实物测试：小车行为与改动前一致（循迹/盲开/停车均正常）

## Blocked by

- Issue 08a ✅ — 已完成 (`9d37476`)
- Issue 08b ✅ — 已完成 (`dacd8d7`)
- Issue 08c ✅ — 已完成 (`da3448a`)

## Status

✅ 全部子 issue 已实施。ISR ~80 行 → ~45 行 (-44%)，仅保留采集职责。
