# Issue 09: task.c 与 car_control.c 职责边界澄清 — 消除 extern 泄漏

## Parent

无 — 架构审查发现

## What

当前 `task.c` 通过 extern 直接读写 `car_control.c` 拥有的全局变量：

```c
// task.c 隐式依赖（通过 main.h / car_control.h 的 extern 声明）
extern float   target_angle;   // car_control.c 定义
extern int16_t base_speed;     // car_control.c 定义
extern int16_t ff_diff;        // car_control.c 定义
extern float   current_angle;  // main.c 定义
```

同时 `car_control.c` 内部又 extern 了 `lap_count`（来自 main.c）。

**问题**：两个模块双向依赖、交叉读写同一个全局变量，导致：
- 改一个模块的值可能被另一个模块意外覆盖（如 `target_angle` 被 ISR 和 task 函数同时写入）
- 调试困难——不知道谁在什么时候改了 `base_speed`
- 单元测试不可能（必须链接整个工程）

### 现实评估

Issue 07 已经将 TIM6 ISR 内的数据流包装为 `Car_SensorInputs → Car_ControlOutputs`，消除了 ISR 侧的 5 条 extern。但 **task.c 侧仍未解决**——task 函数作为"指挥官"，需要告诉 car_control "现在用什么角度、多快、多少差速"。

当前这本质上是一个 **写入者模型**：
- **写入者**: task.c（`Car_StartLine()` / `Car_SetSpeed()` / `Car_SetTargetAngle()` / `Car_SetFFDiff()`）
- **读取者**: car_control.c `Car_ControlLoop()`（ISR 每 tick 消费）
- **写入者2**: car_control.c 自身（`Car_ControlLoop` 中 CTRL_LINE 有线分支写 `target_angle = in.current_angle`）

ISR 侧已通过 struct 解耦。task 侧再包一层 struct 性价比不高——task 函数本质上就是"设置目标参数"，`Car_StartLine()` 已经是最简洁的 API。

### 推荐方案：不引入新 struct，而是收紧访问控制

**核心原则**：task.c 只能通过 `car_control.h` 公开的 API 函数写入，绝不直接 extern 读/写内部变量。

当前状态检查：

| 变量 | task.c 使用方式 | 是否合规 |
|------|----------------|---------|
| `target_angle` | 仅通过 `Car_StartLine()` / `Car_SetTargetAngle()` 写入 | ✅ 已合规 |
| `base_speed` | 仅通过 `Car_StartLine()` / `Car_SetSpeed()` 写入 | ✅ 已合规 |
| `ff_diff` | 仅通过 `Car_StartLine()` / `Car_SetFFDiff()` 写入 | ✅ 已合规 |
| `current_angle` | `YawTrack_Reset(current_angle)` 读取 | ⚠️ 可接受（只读，传感器数据） |

实际上 **Issue 06 重构已经解决了大部分问题**——task.c 不再直接 `target_angle = xxx`，而是通过 `Car_StartLine()` 一站式设置。

### 剩余动作

1. **移除 `car_control.h` 中的 extern 声明**（line 35-37）：

```c
// 删除这三行——task.c 不需要直接访问这些变量
extern float   target_angle;
extern int16_t base_speed;
extern int16_t ff_diff;
```

→ 改为 `car_control.c` 内部 `static`，对外只暴露 getter：

```c
// car_control.h 新增
float   Car_GetTargetAngle(void);
int16_t Car_GetBaseSpeed(void);
int16_t Car_GetFFDiff(void);
```

2. **检查 task.c 是否还有直接读写这些变量的代码**：

```c
// task.c 中唯一可疑的 extern 引用来自 main.h
// 确认后改为通过 Car_* API 访问
```

3. **`lap_count` 搬家**：`lap_count` 当前在 main.c 定义，被 task.c extern。应移到 task.c 定义，main.c 通过 `extern` 读取（或提供 getter）。

### 不改的范围

- **不在 task.c ↔ car_control.c 之间引入第三个 struct**。Issue 07 的 Car_SensorInputs/Car_ControlOutputs 已经覆盖了 ISR ↔ car_control 的边界，task ↔ car_control 的边界用函数 API 已经足够清晰。
- **不强制 task 函数传参风格**。`Car_StartLine(angle, speed, diff)` 三参数已经够用，不需要再包一层 task 专用 struct。

## Acceptance criteria

- [ ] 编译 0 errors 0 warnings
- [ ] `car_control.h` 不再包含 `extern float target_angle` / `extern int16_t base_speed` / `extern int16_t ff_diff`
- [ ] `car_control.c` 中 `target_angle` / `base_speed` / `ff_diff` 改为 `static`，新增 3 个 getter
- [ ] task.c 不再通过 extern 直接读写 car_control 内部变量
- [ ] `lap_count` 移到 task.c 定义，main.c 通过 extern 读取
- [ ] 实物测试：Task 1-5 行为与改动前一致

## Blocked by

None — 可独立实施。建议在 Issue 08（ISR 卸载）之前做，改完后 ISR 卸载时边界更清晰。
