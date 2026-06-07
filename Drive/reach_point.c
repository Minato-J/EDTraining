/*
 * reach_point.c — 声光指示模块 (Issue 03)
 * 移植自 training/hardware/ReachPiont.c
 * 到达关键节点时 PE14/PE15 输出 200ms 低电平脉冲
 */
#include "reach_point.h"
#include "main.h"

#define REACH_POINT_DURATION_TICKS 10  // 10 ticks × 20ms = 200ms

static volatile uint8_t trigger_active = 0;
static volatile uint8_t tick_counter = 0;

void ReachPoint_Init(void) {
    // R2: GPIO 输出模式初始化 — PE14/PE15 默认复位为输入，需显式配置
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_14 | GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    // PE14, PE15 初始高电平 (熄灭/静音)
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_14, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_15, GPIO_PIN_SET);
    trigger_active = 0;
    tick_counter = 0;
}

void ReachPoint_Trigger(uint8_t state) {
    if (state == 0) {
        // ON: 低电平点亮 LED/蜂鸣器
        HAL_GPIO_WritePin(GPIOE, GPIO_PIN_14, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOE, GPIO_PIN_15, GPIO_PIN_RESET);
        trigger_active = 1;
        tick_counter = 0;
    } else {
        // OFF: 高电平熄灭
        HAL_GPIO_WritePin(GPIOE, GPIO_PIN_14, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOE, GPIO_PIN_15, GPIO_PIN_SET);
        trigger_active = 0;
    }
}

void ReachPoint_Tick(void) {
    if (!trigger_active) return;
    tick_counter++;
    if (tick_counter >= REACH_POINT_DURATION_TICKS) {
        ReachPoint_Trigger(1);  // 200ms 自动恢复
    }
}
