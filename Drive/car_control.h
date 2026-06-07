/*
 * car_control.h — 显式控制模式模块 (Issue 06)
 * 移植自 training/hardware/CarDrive.c ctrl_mode 体系
 * 将分散在 main.c ISR 和 task.c 中的控制逻辑收敛为 4 种显式模式
 */
#ifndef __CAR_CONTROL_H
#define __CAR_CONTROL_H

#include <stdint.h>

// === 控制模式枚举 ===
typedef enum {
    CTRL_PARK     = 0,  // 停车：PWM=0，清零累加器
    CTRL_STRAIGHT = 1,  // 直行：速度PI + 小角度PD补偿，不依赖K230
    CTRL_LINE     = 2,  // 循迹：K230转弯量 + 丢线盲开降级 + 盲开超时保护
    CTRL_TURN     = 3   // 原地旋转：纯角度P控制，base_speed自动归零
} ctrl_mode_t;

// === 全局状态（car_control 模块拥有，task.c 只读） ===
extern float   target_angle;
extern int16_t base_speed;
extern int16_t ff_diff;

// === 模式切换 API ===
void Car_Init(void);                    // 上电初始化，默认 CTRL_PARK
void Car_ControlLoop(void);             // TIM6 ISR 每 tick 调用，根据 ctrl_mode 分发
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

#endif
