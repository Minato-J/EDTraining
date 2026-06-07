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
// 全局状态（car_control 模块拥有，原散布于 main.c 和 task.c）
// ================================================================
float   target_angle = 0.0f;
int16_t base_speed   = 0;
int16_t ff_diff      = 0;

// ================================================================
// 内部状态
// ================================================================
static ctrl_mode_t ctrl_mode = CTRL_PARK;
static volatile uint8_t blind_ticks = 0;       // 盲开超时计数器（原 main.c 文件级变量）
static float turn_out_smooth = 0.0f;           // 转弯量低通滤波值（原 main.c 文件级变量）

// ================================================================
// 外部依赖（由 main.c / task.c / k230_track.c 提供）
// ================================================================
extern float current_angle;                    // main.c 角度过滤器输出
extern PID_TypeDef pid_angle;                  // main.c 角度环 PID
extern int16_t target_v_left;                  // main.c 左轮目标速度
extern int16_t target_v_right;                 // main.c 右轮目标速度
extern volatile uint16_t task_running;         // task.c 任务运行标志

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

void Car_Init(void) {
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

// ================================================================
// 核心控制循环（从 main.c TIM6 ISR 移入，每次 tick 调用）
// ================================================================

void Car_ControlLoop(void) {
    // ---- CTRL_PARK: 停车 ----
    if (ctrl_mode == CTRL_PARK) {
        target_v_left  = 0;
        target_v_right = 0;
        return;
    }

    float last_turn_out = 0.0f;  // 替代原 main.c 局部变量 turn_out

    // ---- CTRL_TURN: 原地旋转（纯角度P控制） ----
    if (ctrl_mode == CTRL_TURN) {
        blind_ticks = 0;
        float error = angle_normalize(target_angle - current_angle);
        last_turn_out = PID_Compute(&pid_angle, error, 0);
    }
    // ---- CTRL_STRAIGHT: 直行（速度PI + 小角度PD补偿，不读K230） ----
    else if (ctrl_mode == CTRL_STRAIGHT) {
        float error = angle_normalize(target_angle - current_angle);
        last_turn_out = PID_Compute(&pid_angle, error, 0) * 0.5f;  // 小补偿系数
    }
    // ---- CTRL_LINE: 循迹（K230 + 双模切换 + 盲开超时保护） ----
    else if (ctrl_mode == CTRL_LINE) {
        if (k230_data_valid && line_sensor_data != 0x00) {
            // 有线：K230 循迹
            blind_ticks = 0;
            target_angle = current_angle;  // H0 特有：同步防抽搐
            last_turn_out = (float)K230_Get_Turn_Speed(line_sensor_data);
        } else {
            // 丢线：盲开降级（角度环保持 target_angle）
            blind_ticks++;
            if (blind_ticks > 50) {  // 连续 1 秒无视野 → 紧急停车
                Car_Stop();
                task_running = 0;
                return;
            }
            float error = angle_normalize(target_angle - current_angle);
            last_turn_out = PID_Compute(&pid_angle, error, 0);
        }
    }

    // === 公共后处理：H0 保留优势 ===
    // turn_out 平滑滤波 (0.3*old + 0.7*new)
    turn_out_smooth = 0.3f * turn_out_smooth + 0.7f * last_turn_out;
    // 前馈差速合成
    target_v_left  = base_speed + ff_diff - (int16_t)turn_out_smooth;
    target_v_right = base_speed - ff_diff + (int16_t)turn_out_smooth;
}
