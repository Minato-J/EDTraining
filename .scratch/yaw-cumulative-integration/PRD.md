# PRD: yaw_cumulative 弯道出口判定模块

## 来源

基于 `.docs/Analysis/项目对比分析_H0_vs_training.md` 第 10.5 节改进建议第 1 条：引入 training 项目的 `yaw_cumulative` 辅助弯道判定，作为 count 节点计数的二级冗余校验。

## 背景

H0 当前**只靠 `count`**（摄像头黑线边沿检测）判定到达节点。`count` 在反光、阴影、污渍、弯道弧度平缓等场景下可能漏检或误触发。

training 项目验证了 `yaw_cumulative`（累计航向变化量）作为弯道出口判定的可靠性——它是物理量，不依赖光照条件。

## 目标

在 H0 现有模块化架构下，新增独立的 `Drive/yaw_track.c/h` 模块，实现航向角度累计追踪，融入 TIM6 中断周期，为所有任务的弯道段提供基于角度累计的节点到达二级判定。

## 设计原则

- **纯增量**：新增代码不删除、不破坏任何现有逻辑
- **模块化**：独立 .c/.h 文件，符合 H0 `Drive/` 层规范
- **最小侵入**：main.c 只加 1 行调用，task.c 只加少量复位+校验逻辑
- **冗余兜底**：count 依然为主判定路径，yaw_cumulative 为安全网

## API 设计

```c
void YawTrack_Reset(void);                          // 进入弯道前清零
void YawTrack_Update(float current_yaw);            // 每 TIM6 周期调用
float YawTrack_GetCumulative(void);                 // 查询累计值（度）
uint8_t YawTrack_IsCurveDone(float threshold_deg);  // 累计 > 阈值返回 1
```

## 数据流

```
TIM6 中断 (20ms)
  → 角度突变过滤器 → current_angle
  → YawTrack_Update(current_angle)   // 新增: 累加 delta
  → 双模切换（循迹/盲开/旋转）
  → ...
```

## 成功标准

- [ ] 小车在弯道中，yaw_cumulative 随航向变化持续增长
- [ ] YawTrack_Reset() 可正确清零并记录新的起始角度
- [ ] ±180° 跳变正确处理（例如从 +179° 转到 -179°，delta=+2° 而非 -358°）
- [ ] Task 2~5 所有弯道 case 在 count 漏触发时，yaw_cumulative 可兜底推进状态
- [ ] 不影响 Task 1（直线）正常运行

## Issues

| # | 标题 | 阻塞 |
|---|------|------|
| 01 | 新建 yaw_track 模块 + 核心算法 | 无 |
| 02 | 接入 TIM6 中断 + 发车 Reset | 01 |
| 03 | Task 2 外圈加 yaw_cumulative 二级校验 | 02 |
| 04 | Task 3/4/5 全部弯道段加兜底 | 03 |
| 05 | yaw_track.c 头部实现逻辑文档 + 注释 | 01~04 |
