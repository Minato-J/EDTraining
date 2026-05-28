#ifndef __TASK_H
#define __TASK_H

#include "stdint.h"

// 暴露? main.c 和其他文件使用的全局?量
extern uint16_t selected_task;
extern uint16_t task_running;
extern uint8_t current_state;
extern uint8_t last_line_status;
extern uint8_t count;

// 核心函??明
void Task_Manager_Init(void); // 任?管理器初始化 (?机?用一次)
void Task_Key_Scan(void);     // 按??描函? (放在 while(1) 里)
void Task_Dispatcher(void);   // 任?分?器 (放在 while(1) 里)

#endif