#include "pid.h"
#include <math.h>

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