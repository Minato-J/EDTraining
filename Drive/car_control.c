/*
 * car_control.c — 显式控制模式实现 (Issue 06)
 * 移植自 training/hardware/CarDrive.c ctrl_mode 体系
 *
 * Car_ControlLoop() 替代 main.c TIM6 ISR 中 ~50 行双模切换逻辑。
 * 保留 H0 优势：盲开超时保护、turn_out 平滑滤波、双模切换、ff_diff 前馈差速。
 */
#include "car_control.h"
#include "main.h"
#include "pid.h"
#include "k230_track.h"
#include "wit_imu.h"
#include <math.h>

// ================================================================
// 全局状态（car_control 模块拥有，外部只读通过 getter，仅 API 函数写入）
// ================================================================
static float   target_angle = 0.0f;
static int16_t base_speed   = 0;
static int16_t ff_diff      = 0;

// ================================================================
// 内部状态
// ================================================================
static PID_TypeDef *angle_pid_ptr = NULL;        // Car_Init 注入，替代 extern pid_angle
static volatile ctrl_mode_t ctrl_mode = CTRL_PARK;  // volatile: 主循环写，ISR 读
static volatile uint8_t blind_ticks = 0;       // 盲开超时计数器（原 main.c 文件级变量）
static volatile float turn_out_smooth = 0.0f;       // volatile: ISR 写，主循环读（DEBUG_UART + Start 清零）

// ================================================================
// 外部依赖 — 已消除 extern 全局变量泄漏 (Issue 07)
//   current_angle / line_sensor_data / k230_data_valid → Car_SensorInputs in
//   target_v_left / target_v_right / emergency_stop        → Car_ControlOutputs out
//   pid_angle                                             → angle_pid_ptr (Init 注入)
// ================================================================

// ================================================================
// 辅助函数
// ================================================================
static float angle_normalize(float diff) {
    while (diff > 180.0f)  diff -= 360.0f;
    while (diff < -180.0f) diff += 360.0f;
    return diff;
}

// ================================================================
// 模式切换 API
// ================================================================

void Car_Init(PID_TypeDef *angle_pid) {
    angle_pid_ptr = angle_pid;
    ctrl_mode = CTRL_PARK;
    target_angle = 0.0f;
    base_speed   = 0;
    ff_diff      = 0;
    blind_ticks  = 0;
    turn_out_smooth = 0.0f;
}

void Car_ResetState(void) {
    blind_ticks = 0;
    turn_out_smooth = 0.0f;
    // 注意：PID 重置由 main.c ControlState_Reset() 负责
}

void Car_Stop(void) {
    ctrl_mode  = CTRL_PARK;
    base_speed = 0;
    ff_diff    = 0;
    blind_ticks = 0;
    turn_out_smooth = 0.0f;
}

void Car_StartLine(float target_deg, int16_t speed, int16_t diff) {
    ctrl_mode    = CTRL_LINE;
    target_angle = target_deg;
    base_speed   = speed;
    ff_diff      = diff;
    blind_ticks  = 0;
    turn_out_smooth = 0.0f;
}

void Car_StartStraight(float target_deg, int16_t speed) {
    ctrl_mode    = CTRL_STRAIGHT;
    target_angle = target_deg;
    base_speed   = speed;
    ff_diff      = 0;
    blind_ticks  = 0;
}

void Car_TurnTo(float target_deg) {
    ctrl_mode    = CTRL_TURN;
    target_angle = target_deg;
    base_speed   = 0;
    ff_diff      = 0;
    blind_ticks  = 0;
}

// ================================================================
// 运行时微调
// ================================================================

void Car_SetSpeed(int16_t speed) {
    base_speed = speed;
}

void Car_SetTargetAngle(float angle) {
    target_angle = angle;
}

void Car_SetFFDiff(int16_t diff) {
    ff_diff = diff;
}

ctrl_mode_t Car_GetMode(void) {
    return ctrl_mode;
}

float Car_GetTurnOutSmooth(void) {
    return turn_out_smooth;
}

// === getter: 外部只读访问内部状态 ===
float   Car_GetTargetAngle(void) { return target_angle; }
int16_t Car_GetBaseSpeed(void)   { return base_speed; }
int16_t Car_GetFFDiff(void)      { return ff_diff; }

// ================================================================
// 核心控制循环（TIM6 ISR 每 tick 调用，~20ms）
// Issue 07: 签名改为 Car_ControlLoop(Car_SensorInputs) → Car_ControlOutputs
//           消除 5 条 extern 跨模块全局变量泄漏
// ================================================================

Car_ControlOutputs Car_ControlLoop(Car_SensorInputs in) {
    Car_ControlOutputs out = {0};  // 零初始化所有输出字段

    // ---- CTRL_PARK: 停车 ----
    if (ctrl_mode == CTRL_PARK) {
        out.target_v_left  = 0;
        out.target_v_right = 0;
        return out;
    }

    float last_turn_out = 0.0f;  // 替代原 main.c 局部变量 turn_out

    // ---- CTRL_TURN: 原地旋转（纯角度P控制） ----
    if (ctrl_mode == CTRL_TURN) {
        blind_ticks = 0;
        float error = angle_normalize(target_angle - in.current_angle);
        last_turn_out = PID_Compute(angle_pid_ptr, error, 0);
    }
    // ---- CTRL_STRAIGHT: 直行（速度PI + 小角度PD补偿，不读K230） ----
    else if (ctrl_mode == CTRL_STRAIGHT) {
        float error = angle_normalize(target_angle - in.current_angle);
        last_turn_out = PID_Compute(angle_pid_ptr, error, 0) * 0.5f;  // 小补偿系数
    }
    // ---- CTRL_LINE: 循迹（K230 + 双模切换 + 盲开超时保护） ----
    else if (ctrl_mode == CTRL_LINE) {
        if (in.k230_data_valid && in.line_sensor_data != 0x00) {
            // 有线：K230 循迹
            blind_ticks = 0;
            target_angle = in.current_angle;  // H0 特有：同步防抽搐
            last_turn_out = (float)K230_Get_Turn_Speed(in.line_sensor_data);
        } else {
            // 丢线：盲开降级（角度环保持 target_angle）
            blind_ticks++;
            if (blind_ticks > 50) {  // 连续 1 秒无视野 → 紧急停车
                Car_Stop();
                out.emergency_stop = 1;      // 通知 ISR 设 task_running=0
                out.target_v_left  = 0;      // R3: return 前清零，不等下一帧 CTRL_PARK
                out.target_v_right = 0;
                return out;
            }
            float error = angle_normalize(target_angle - in.current_angle);
            last_turn_out = PID_Compute(angle_pid_ptr, error, 0);
        }
    }

    // === 公共后处理：H0 保留优势 ===
    // turn_out 平滑滤波 (0.3*old + 0.7*new)
    turn_out_smooth = 0.3f * turn_out_smooth + 0.7f * last_turn_out;
    // 前馈差速合成
    out.target_v_left  = base_speed + ff_diff - (int16_t)turn_out_smooth;
    out.target_v_right = base_speed - ff_diff + (int16_t)turn_out_smooth;

    return out;
}
