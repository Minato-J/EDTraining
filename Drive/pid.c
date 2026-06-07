#include "pid.h"
#include <math.h>

// ---- 位置式 PID (角度环 PD 使用，保留不变) ----

void PID_Init(PID_TypeDef *pid, float p, float i, float d, float max) {
    pid->Kp = p; pid->Ki = i; pid->Kd = d;
    pid->out_max = max;
    pid->target = 0; pid->error = 0; pid->last_error = 0;
    pid->integral = 0; pid->output = 0;
}

float PID_Compute(PID_TypeDef *pid, float target, float measured) {
    // NaN 哨兵: NaN 一旦进入 integral 则永久污染，唯一恢复是硬件复位
    // NaN > X / NaN < X / NaN == X 全部返回 false，会绕过所有限幅守卫
    if (isnan(target) || isnan(measured)) {
        return pid->output;  // 保持上次输出，不更新内部状态
    }

    pid->target = target;
    pid->error = pid->target - measured;
    
    // ?���֥[
    pid->integral += pid->error;
    
    // ??��?����?�M�]���T�^
    if (pid->integral > pid->out_max) pid->integral = pid->out_max;
    if (pid->integral < -pid->out_max) pid->integral = -pid->out_max;
    
    // ?��?�X
    pid->output = (pid->Kp * pid->error) + 
                  (pid->Ki * pid->integral) + 
                  (pid->Kd * (pid->error - pid->last_error));
    
    pid->last_error = pid->error;
    
    // ?�X���T (��t�A�� ARR=100)
    if (pid->output > pid->out_max) pid->output = pid->out_max;
    if (pid->output < -pid->out_max) pid->output = -pid->out_max;

    return pid->output;
}

// ---- 增量式速度 PI (Issue 01) ----
// 公式: bias = target - measured
//       pwm_acc += Kp*(bias - last_bias) + Ki*bias
//       clamp(pwm_acc, 0, out_max)
// 参考: training/hardware/CarDrive.c velocity_pi()
#ifdef PID_USE_VELOCITY_FORM

void PID_Velocity_Init(PID_Velocity_TypeDef *pid, float p, float i, int16_t max) {
    pid->Kp = p;
    pid->Ki = i;
    pid->out_max = max;
    pid->pwm_acc = 0;
    pid->last_bias = 0;
}

float PID_Velocity_Compute(PID_Velocity_TypeDef *pid, int target, int measured) {
    int bias = target - measured;
    pid->pwm_acc += (int)(pid->Kp * (float)(bias - pid->last_bias)
                        + pid->Ki * (float)bias);
    pid->last_bias = bias;

    // 限幅 [0, out_max]: 增量式 PI 只能输出正向 PWM
    if (pid->pwm_acc > pid->out_max) pid->pwm_acc = pid->out_max;
    if (pid->pwm_acc < 0)            pid->pwm_acc = 0;

    return (float)pid->pwm_acc;
}

#endif