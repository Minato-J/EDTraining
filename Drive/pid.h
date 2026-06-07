#ifndef __PID_H
#define __PID_H

#include "main.h"

// PID 位置式结构体（角度环 PD 使用）
typedef struct {
    float target;     // 目标
    float Kp, Ki, Kd; // 参数
    float error;      // 当前误差
    float last_error; // 上一次误差
    float integral;   // 积分
    float output;     // 输出
    float out_max;    // 输出最大限幅
} PID_TypeDef;

// 函数声明
void PID_Init(PID_TypeDef *pid, float p, float i, float d, float max);
float PID_Compute(PID_TypeDef *pid, float target, float measured);

// ---- 增量式速度 PI (Issue 01: velocity-form PI, 模式切换天然平滑) ----
#ifdef PID_USE_VELOCITY_FORM
typedef struct {
    float Kp, Ki;         // PI 参数
    int16_t pwm_acc;      // PWM 累加器（增量叠加）
    int     last_bias;    // 上一次偏差
    int16_t out_max;      // 输出上限 (PWM 最大值)
} PID_Velocity_TypeDef;

void PID_Velocity_Init(PID_Velocity_TypeDef *pid, float p, float i, int16_t max);
float PID_Velocity_Compute(PID_Velocity_TypeDef *pid, int target, int measured);
#endif

#endif