# yaw-cumulative-integration — Code Review Changelog

**审查日期**: 2026-06-04  
**审查范围**: `git diff HEAD`（5 个文件，+90/-16 行）  
**审查级别**: max（5 correctness + 3 cleanup + 1 altitude → 1-vote verify → gaps sweep）

---

## 审查统计

| 指标 | 数量 |
|------|------|
| 独立审查角度 | 9 (A→I) |
| 验证阶段投票 | 5 个 correctness findings |
| 缺口扫描新发现 | 7 个 |
| 最终报告发现 | 8 个 |
| CONFIRMED | 6 个 |
| PLAUSIBLE | 2 个 |

---

## 发现清单（按严重性排序）

### 1. CONFIRMED — count/count_debounce/task_running 缺少 volatile

**文件**: `Drive/task.c:59,67,70` + `Drive/task.h:11-14`

ARM Compiler 5 在 `-O2` 下有权限将这些 ISR ↔ 主循环共享变量提升到寄存器中。ISR 写入内存但主循环读到的是寄存器中的过期副本。

- `count` — ISR + 主循环 YawTrack 路径双重递增
- `count_debounce` — ISR 和主循环同时写入
- `task_running` — ISR 写入（盲开超时停车），主循环读取

**最坏场景**: 盲开超时 ISR 设 `task_running=0` → 主循环寄存器缓存为 1 → 电机已停但任务函数持续输出非零 PID 目标。

**修复**: 全部加 `volatile` 修饰符（与 yaw_track.c 中的处理一致）。

---

### 2. CONFIRMED — NaN 零防护：全链路无 isnan() 检查

**文件**: `Core/Src/main.c:259→270→280/297/306→329/338`（整条控制链）

IEEE 754 NaN 的一个关键属性：`NaN > X`、`NaN < X`、`NaN == X` **全部返回 false**。这导致所有守卫条件对 NaN 完全透明。

**传播链**:
```
raw_yaw=NaN → fabsf(NaN)>30=false（绕过过滤器）
→ current_angle=NaN → YawTrack_Update: yaw_cumulative 永久 NaN
→ PID_Compute: integral+NaN=NaN（PID 永久失能）
→ (int16_t)NaN 未定义行为（ARMCC 产生 0 或 -32768）
→ 电机暴冲或锁死
```

**唯一恢复手段**: 硬件复位。NaN 一旦进入持久状态（integral、last_error、yaw_cumulative、yaw_prev、turn_out_smooth），再也没有任何代码能将其清除。

**威胁入口**: UART 电磁干扰（JY901S 1/256 帧通过弱校验 + 非 NaN 入口）、RAM 位翻转（电机 EMI）、栈溢出。虽然 UART 解析路径不直接产生 NaN，但入口不是零。

**修复**: 在 raw_yaw 计算后加 `isnan()` 哨兵；在 PID 输入前检查。

---

### 3. CONFIRMED — float last_valid_angle!=0 哨兵被 0° 航向击穿

**文件**: `Core/Src/main.c:263`

使用 `last_valid_angle != 0` 判断"是否已有首个有效读数"。但当小车航向恰好经过 0°（Task 1/2 中 `target_angle=0` 是正常稳态），`last_valid_angle` 被更新为 `0.0f`，哨兵失效。

**触发**: 0° 稳态航向 → IMU 跳变 100° → `fabsf(100)>30` 为真，`0.0f!=0` 为假 → `if` 整体为 false → 尖峰绕过过滤器。

**修复**: 用独立的 `static uint8_t angle_initialized` 布尔标志。

---

### 4. CONFIRMED — static blind_ticks 正常任务结束时未归零

**文件**: `Core/Src/main.c:275`

紧急停车路径（`blind_ticks>50 → task_running=0`）在第 294 行显式归零了 `blind_ticks`。但任务**正常结束**（task.c 将 `task_running` 设为 0）时，`blind_ticks` 停在 0-50 之间的任意值。下次 Start 后从该值继续计数。

**最坏场景**: `blind_ticks=49`、任务正常结束 → 下次启动首个 `line_sensor_data==0x00` 帧 → blind_ticks 从 50 开始 → 仅 1 tick (20ms) 即触发紧急停车。

**修复**: 将 `blind_ticks` 提升为文件级变量，在 `Task_Key_Scan` Start 处理中显式归零。

---

### 5. CONFIRMED — static turn_out_smooth 任务重启时保留旧值

**文件**: `Core/Src/main.c:313`

`turn_out_smooth` 是 ISR 内部 static 变量。`task_running==0` 时代码块被跳过，值冻结。下次 Start 后首次进入，EMA `0.3*old+0.7*new` 仍使用旧值。

**影响**: 新任务前 3-5 个控制周期 (~60-100ms) 差速合成被旧转弯量污染。

**修复**: 在任务启动路径中（或 ISR 中检测到 task_running 从 0→1 跳变时）显式清零。

---

### 6. CONFIRMED — YawTrack_IsCurveDone(180°) 在 Task 3/4 中阈值过高

**文件**: `Drive/task.c:315,337,359,381,409,430,451,472,500,521,543,564`

实际航向变化 vs 180° 阈值:

| 路段 | 理论偏航变化 | 阈值 | 安全网有效？ |
|------|-------------|------|------------|
| A→C 对角 | ~38° | 180° | **绝不可能触发** |
| C→B 弧段 | ~38° | 180° | **绝不可能触发** |
| B→D 对角 | ~144° | 180° | 边缘 (36° 余量) |
| D→A 弧段/直道 | ~36° 或 ~0° | 180° | **绝不可能触发** (直道为 0°) |

一旦 K230 count 漏检，YawTrack 安全网**在正常行驶中永远不会触发**。小车将以当前 target_angle 直冲出场地。

**修复**: 根据各弧段实际角度变化设置合理阈值（小弧段 ~50°，大对角 ~160°），直道段改用时间超时。

---

### 7. PLAUSIBLE — YawTrack count++ 与 count_debounce 间的指令级竞争窗口

**文件**: `Drive/task.c:150,216,619`

主循环 `count++; count_debounce=DEBOUNCE_INIT;` 两条 C 语句之间有 ~3-5 条汇编指令（~30ns @168MHz）。若 TIM6 ISR 恰好在此窗口中触发，`count_debounce` 仍为 0，ISR 执行第二次 `count++`。

**概率**: ~30ns / 20ms = 1.5×10⁻⁶（单次 YawTrack 事件）。  
**后果**: count 跳 2，跳过整条路段。  
**修复**: 用 `__disable_irq()/__enable_irq()` 包裹，或将 count 修改路径统一到 ISR 中。

---

### 8. PLAUSIBLE — uint8_t count 在 Task 4 长时间振荡下可溢出

**文件**: `Drive/task.c:70`

`uint8_t count` 最大 255。Task 4 中若节点反复振荡（ISR 每 200ms 递增一次）持续 51 秒 → count 从 255 翻转到 0 → `count>=N` 失效。若同时 YawTrack 兜底阈值过高无法触发 → 永久卡死。

**修复**: 改为 `uint16_t` 或在 ISR 递增处加 `if (count < 255)` 饱和检查。

---

## 非缺陷发现（文档记录，本次不改）

以下来自 cleanup/altitude 审查角度，不构成正确性缺陷：

| 类型 | 发现 | 影响 |
|------|------|------|
| 代码重复 | while 角度归一化循环出现 4 次 (main.c×3 + yaw_track.c×1) | 维护风险 |
| 代码重复 | Task 4 的 12 对状态高度重复 (~280 行复制) | 可读性/可维护性 |
| 代码重复 | `count_debounce = DEBOUNCE_INIT` 模式 4 处重复 | 不统一 |
| 架构 | YawTrack 生命周期与各任务手动耦合，而非分发层自动管理 | 新任务需复制样板代码 |
| 调试残留 | OLED 刷新每 10ms 执行一次 (~96ms 软件 I2C)，应 `#ifdef DEBUG_OLED` | 竞赛日主循环抖动 |
| 静态变量未重置 | Task 2/3 的 `t2_last_count`/`t3_last_count` 在停车时未像 Task 5 那样重置为 -1 | 风格不一致，当前无实际危害 |
| 停车时 PID 空转 | `task_running==0` 时 PID_Compute 仍然被调用 | 微小 CPU 浪费 |
| 坐标不一致 | `RIGHT_HALF_CIRCLE=-185°` 超出 IMU 有效范围 [-180,+180] | 脆弱，依赖归一化修正 |

---

## 修复优先级建议

| 优先级 | 发现 | 理由 |
|--------|------|------|
| P0 | #1 volatile | 编译器优化可能使盲开超时完全失效 |
| P0 | #3 float 哨兵 | 一次 IMU 跳变即可导致小车脱线 |
| P1 | #4 blind_ticks | 正常比赛场景中可能触发 |
| P1 | #2 NaN 传播 | 潜在灾难性后果（但触发概率低） |
| P1 | #6 阈值过高 | 安全网在大多数弧段上根本不存在 |
| P2 | #5 turn_out_smooth | 短暂影响 (60-100ms)，不影响长期运行 |
| P2 | #7 指令级竞争 | 概率极低，但不是零 |
| P2 | #8 count 溢出 | 需特定振荡条件，概率低 |

---

*审查由 Claude Code `/code-review max` 驱动，9 角度 finder + 验证 + gaps sweep。*
