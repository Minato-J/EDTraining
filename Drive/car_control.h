/*
 * car_control.h — 显式控制模式模块 (Issue 06)
 * 移植自 training/hardware/CarDrive.c ctrl_mode 体系
 * 将分散在 main.c ISR 和 task.c 中的控制逻辑收敛为 4 种显式模式
 */
#ifndef __CAR_CONTROL_H
#define __CAR_CONTROL_H

#include <stdint.h>
#include "pid.h"

// === 控制模式枚举 ===
typedef enum {
    CTRL_PARK     = 0,  // 停车：PWM=0，清零累加器
    CTRL_STRAIGHT = 1,  // 直行：速度PI + 小角度PD补偿，不依赖K230
    CTRL_LINE     = 2,  // 循迹：K230转弯量 + 丢线盲开降级 + 盲开超时保护
    CTRL_TURN     = 3   // 原地旋转：纯角度P控制，base_speed自动归零
} ctrl_mode_t;

// === 每 tick 输入（ISR 在调用前组装，消除 extern 跨模块泄漏）===
typedef struct {
    float   current_angle;      // main.c 角度过滤器输出
    uint8_t line_sensor_data;   // K230 6 路灰度原始值
    uint8_t k230_data_valid;    // K230 帧新鲜度标志
} Car_SensorInputs;

// === 每 tick 输出（ISR 在调用后消费）===
typedef struct {
    int16_t target_v_left;      // 左轮目标速度 → speed PID
    int16_t target_v_right;     // 右轮目标速度 → speed PID
    uint8_t emergency_stop;     // 盲开超时 → ISR 设 task_running=0
} Car_ControlOutputs;

// === 模式切换 API ===
void Car_Init(PID_TypeDef *angle_pid);   // 上电初始化（注入角度环 PID 指针），默认 CTRL_PARK
Car_ControlOutputs Car_ControlLoop(Car_SensorInputs in);  // TIM6 ISR 每 tick 调用，根据 ctrl_mode 分发
void Car_Stop(void);                    // 切 CTRL_PARK + 清零累加器
void Car_ResetState(void);              // Start 时清零内部累积状态（替代 ControlState_Reset 中 car_control 部分）

// === 模式入口（一站式设置，Task 函数一行调用） ===
void Car_StartLine(float target_deg, int16_t speed, int16_t diff);
    // 切 CTRL_LINE，设 target_angle/base_speed/ff_diff，清零 turn_out_smooth/blind_ticks

void Car_StartStraight(float target_deg, int16_t speed);
    // 切 CTRL_STRAIGHT，设 target_angle/base_speed，不依赖K230

void Car_TurnTo(float target_deg);
    // 切 CTRL_TURN，设 target_angle，base_speed自动归零，ff_diff归零

// === 运行时微调（不改模式，仅调参） ===
void Car_SetSpeed(int16_t speed);       // 调整当前 base_speed
void Car_SetTargetAngle(float angle);   // 调整当前 target_angle
void Car_SetFFDiff(int16_t diff);       // 调整前馈差速

// === 状态查询 ===
ctrl_mode_t Car_GetMode(void);          // 查询当前控制模式
float Car_GetTurnOutSmooth(void);       // 查询转弯量平滑值（调试用）
float   Car_GetTargetAngle(void);       // 查询当前 target_angle
int16_t Car_GetBaseSpeed(void);         // 查询当前 base_speed
int16_t Car_GetFFDiff(void);            // 查询当前 ff_diff

#endif
