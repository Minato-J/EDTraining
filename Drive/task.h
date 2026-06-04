#ifndef __TASK_H
#define __TASK_H

#include <stdint.h>

// 与 main.c 共享的全局变量
extern uint16_t selected_task;
extern uint16_t task_running;
extern uint8_t current_state;
extern uint8_t last_line_status;
extern uint8_t count;
extern uint8_t lap_count;
extern float current_angle;
extern uint8_t count_debounce;   // H0: 节点计数去抖窗口
extern int16_t ff_diff;          // H0: 前馈差速
extern uint8_t rotating;         // 旋转阶段标志 (1=正在原地旋转, 屏蔽K230)

// 核心函数
void Task_Manager_Init(void); // 任务管理器初始化
void Task_Key_Scan(void);     // 按键扫描 (放在 while(1) 中)
void Task_Dispatcher(void);   // 任务分发 (放在 while(1) 中)

#endif