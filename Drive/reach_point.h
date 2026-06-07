#ifndef __REACH_POINT_H
#define __REACH_POINT_H

#include <stdint.h>

void ReachPoint_Init(void);
void ReachPoint_Trigger(uint8_t state);  // 0=ON (low, 点亮), 1=OFF (high, 熄灭)
void ReachPoint_Tick(void);              // TIM6 每 tick 调用，管理 200ms 自动恢复

#endif
