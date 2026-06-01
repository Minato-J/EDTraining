# Issue 008: 盲开超时紧急停车

## Parent

无 — 来自合并方案 §5.3

## What to build

在 TIM6 中断的盲开分支中增加超时保护：当 K230 连续 1 秒返回 `0x00`（全白脱线），小车自动紧急停车并终止任务，防止永久脱线后无限盲开失控。

**文件**: `H0/Core/Src/main.c` — `HAL_TIM_PeriodElapsedCallback()`

### 改动

1. 在 ISR 中新增 `static uint8_t blind_ticks = 0;`
2. 盲开分支 (`line_sensor_data == 0x00`):
   - `blind_ticks++;`
   - 若 `blind_ticks > 50`（50 × 20ms = 1 秒），则 `base_speed = 0; task_running = 0;`
3. K230 循迹分支 (`else`):
   - `blind_ticks = 0;` 清零
4. 附带修复 line 168 的 stray `t` 字符（语法错误）

### 代码位置参考

当前 `main.c:267-277` 为双模切换核心。需在此区域的盲开分支中嵌入计数器和超时判断。

```c
if (line_sensor_data == 0x00)
{
    blind_ticks++;
    if (blind_ticks > 50)
    {
        base_speed = 0;
        task_running = 0;  // 紧急停车
    }
    else
    {
        float angle_error = target_angle - current_angle;
        while (angle_error > 180.0f)  angle_error -= 360.0f;
        while (angle_error < -180.0f) angle_error += 360.0f;
        turn_out = PID_Compute(&pid_angle, angle_error, 0);
    }
}
else
{
    blind_ticks = 0;
    // ... K230 循迹
}
```

## Acceptance criteria

- [ ] 编译 0 errors 0 warnings
- [ ] `blind_ticks` 在盲开分支递增
- [ ] `blind_ticks > 50` 时触发 `base_speed=0; task_running=0`
- [ ] `blind_ticks` 在 K230 分支清零（脱线恢复后重新计时）
- [ ] line 168 stray `t` 字符已修复
- [ ] 逻辑不影响已有的 angle_error 规范化盲开控制

## Blocked by

None — 可立即开始
