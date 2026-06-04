/*
 * ============================================================
 * yaw_track.c — 航向角累计追踪模块（弯道出口判定冗余）
 * ============================================================
 *
 * 【模块目的】
 * 本模块通过逐帧累加陀螺仪 Yaw 角的变化量（yaw_cumulative），
 * 提供一种不依赖摄像头黑线检测的弯道出口判定手段。
 * 在 K230 因反光/阴影/污渍漏检或误报黑线边沿时，
 * yaw_cumulative 可作为二级安全网强制推进任务状态机。
 *
 * 【核心算法】
 * 每 20ms（TIM6 中断周期），Update() 执行以下步骤：
 *
 *   (1) delta = current_yaw - yaw_prev
 *       → 计算本帧相对于上一帧的航向变化量
 *
 *   (2) ±180° 跳变修正：
 *       陀螺仪 Yaw 角范围是 [-180°, +180°]。
 *       例如从 +179° 转到 -179°，实际只顺时针转了 2°，
 *       但 naive 减法会得到 -358°。
 *       修正方法：用 while 循环将 delta 归化到 (-180°, +180°]，
 *       +179° → -179° 的 delta 被修正为 +2°。
 *
 *   (3) yaw_cumulative += fabsf(delta)
 *       → 取绝对值累加，不区分左弯/右弯
 *       这样阈值判断只需检查 >= 200° 或 >= 180°，
 *       无需关心弯道方向。
 *
 *   (4) yaw_prev = current_yaw
 *       → 保存本帧值供下一帧计算 delta 使用
 *
 * 【调用时机】
 * - YawTrack_Reset(current_yaw)
 *     发车时（task.c Start 按键处理）
 *     进入新弯道路段时（各任务 case 首次执行）
 *     传入当前航向角以同步起点，避免首帧注入虚假 delta
 *
 * - YawTrack_Update(current_yaw)
 *     main.c TIM6 中断回调，每 20ms 调用一次
 *     在角度突变过滤器输出 current_angle 之后立即调用
 *
 * - YawTrack_GetCumulative()
 *     OLED 显示（调试用）
 *
 * - YawTrack_IsCurveDone(threshold_deg)
 *     各任务弯道路段的 if 判定
 *     Task 2/5 弧段阈值 200°（接近半圆弧完整角度变化）
 *     Task 3/4 弧段阈值 180°（保守，适配对角弯）
 *
 * 【设计决策】
 * 1. 为什么用 fabsf 取绝对值？
 *    场地既有左弯也有右弯，若保留符号则每个弯道 case 需
 *    不同的阈值比较方向（左弯 >+N 右弯 >-N），代码变复杂。
 *    取绝对值后统一用 ">" 判断，简化逻辑。
 *
 * 2. 为什么不在直道段累计？
 *    直道段 Yaw 角细小漂移（IMU 噪声）会持续积累到
 *    yaw_cumulative。若整个任务期间不 Reset，直道结束后
 *    累计值可能已超过弯道阈值，导致误推进。
 *    因此 Reset() 只在进入弯道路段时调用，直道段不累计。
 *
 * 3. track_enable 的作用？
 *    防止 Update() 在未 Reset 时累加无效数据。
 *    发车前 track_enable==0，Update() 立即返回。
 *    发车 + 进弯道时 Reset() 置 1，开始有效追踪。
 *
 * 【阈值选择经验】
 * - 半圆弧（Task 2 B→C、D→A）：理论航向变化 180°±偏差
 *   使用 200° 阈值提供约 20° 余量，防止过早触发
 * - 对角线弯（Task 3/4 C→B、B→D、D→A）：理论变化约 142°~180°
 *   使用 180° 阈值，由 count 主判定兜底
 *
 * ============================================================
 */

#include "yaw_track.h"
#include <math.h>

// 累计航向变化量（度），取绝对值，不区分左右弯
// volatile: TIM6 ISR 写入，主循环读取，防止编译器缓存
static volatile float yaw_cumulative = 0.0f;
// 上一帧的 Yaw 值，用于计算帧间 delta
static volatile float yaw_prev = 0.0f;
// 使能标志：0=不追踪（直道/停车），1=追踪（弯道中）
static volatile uint8_t track_enable = 0;

/*
 * 重置累计追踪状态
 * 发车时或进入新弯道路段时调用
 * current_yaw: 当前航向角，用于同步起点，避免首帧注入虚假 delta
 */
void YawTrack_Reset(float current_yaw)
{
    yaw_cumulative = 0.0f;
    yaw_prev = current_yaw;  // 同步起点，首帧 delta 恒为 0
    track_enable = 1;        // 开启追踪
}

/*
 * 每帧更新累计角度（TIM6 中断中调用，周期 ~20ms）
 * current_yaw: 经过角度突变过滤后的当前航向角
 */
void YawTrack_Update(float current_yaw)
{
    if (!track_enable) return;

    // 计算本帧航向变化量
    float delta = current_yaw - yaw_prev;
    // ±180° 跳变归化：例如 +179° → -179° 应得到 +2° 而非 -358°
    while (delta > 180.0f)  delta -= 360.0f;
    while (delta < -180.0f) delta += 360.0f;

    // 取绝对值累加，不区分左右弯
    yaw_cumulative += fabsf(delta);
    // 保存本帧值供下一帧计算 delta
    yaw_prev = current_yaw;
}

/*
 * 查询当前累计航向变化量
 * 返回值单位：度
 */
float YawTrack_GetCumulative(void)
{
    return yaw_cumulative;
}

/*
 * 弯道出口判定
 * threshold_deg: 累计角度阈值（度）
 * 返回 1 表示累计角度已超过阈值，弯道应结束
 */
uint8_t YawTrack_IsCurveDone(float threshold_deg)
{
    if (yaw_cumulative > threshold_deg) return 1;
    return 0;
}
