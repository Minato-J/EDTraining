# Issue 007: K230 参数安全加固

## Parent

无 — 来自合并方案 §5.4

## What to build

更新 `K230_Get_Turn_Speed()` 中的转向参数，从 H0 激进值切换为保守值，并对未识别传感器状态安全归零。

**改动集中在 `H0/Drive/k230_track.c` 的 `K230_Get_Turn_Speed()` 函数内**：

1. 微偏~中度 8 个 case：从 (±3 / ±6 / ±8 / ±9) 改为 (±2 / ±4 / ±7 / ±6)
2. 极偏 4 个 case：从 (±18 / ±27) 改为中间值 (±16 / ±20)
3. `default` 分支：从 `turn_speed = last_turn` 改为 `break`（安全归零）
4. `0x00` 全白脱线：保留 `last_turn` 方向惯性（防御性编程）

### 最终参数表

| 传感器值 | 含义 | turn_speed |
|---------|------|-----------|
| 0x0C | 正中 | 0 |
| 0x08 / 0x04 | 微偏 1 位 | ±2 |
| 0x18 / 0x06 | 微偏 2 位 | ±4 |
| 0x1C / 0x0E | 中度 4 位含 3 | ±6 |
| 0x38 / 0x07 | 中度 3 位 | ±7 |
| 0x10 / 0x02 | 重度单侧内路 | ±10 |
| 0x30 / 0x03 | 极偏最外侧 2 位 | ±16 |
| 0x20 / 0x01 | 极偏单边最外 | ±20 |
| 0x00 | 全白脱线 | last_turn (保持惯性) |
| default | 未识别 | 0 (安全归零) |

## Acceptance criteria

- [ ] 编译 0 errors 0 warnings
- [ ] 微偏 case (0x08/0x04) turn_speed = ±2
- [ ] 中度 case (0x1C/0x0E/0x38/0x07) turn_speed = ±6~7
- [ ] 极偏 case (0x30/0x20/0x03/0x01) turn_speed = ±16~20
- [ ] `default: break;` — 未识别传感器值安全归零
- [ ] `case 0x00: turn_speed = last_turn;` — 全白脱线保持惯性

## Blocked by

None — 可立即开始
