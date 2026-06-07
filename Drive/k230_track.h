#ifndef __K230_TRACK_H
#define __K230_TRACK_H

#include "stdint.h"

extern volatile uint8_t line_sensor_data;      // volatile: USART3 ISR 写，TIM6 ISR 读
extern uint8_t k230_rx_data;
extern volatile uint8_t k230_data_valid;   // Issue 02: 帧新鲜度标志
void K230_Parse_Byte(uint8_t byte);
int16_t K230_Get_Turn_Speed(uint8_t sensor_val);
void K230_Timeout_Tick(void);              // Issue 02: 每 TIM6 tick 递增超时计数
void K230_Timeout_Reset(void);             // Issue 02: Start 时清零超时

#endif