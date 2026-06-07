#include "k230_track.h"

uint8_t line_sensor_data = 0x00;
uint8_t k230_rx_data = 0;
volatile uint8_t k230_data_valid = 0;        // Issue 02: 帧新鲜度
static volatile uint8_t k230_timeout_cnt = 0; // Issue 02: 超时计数器
#define K230_TIMEOUT_TICKS 5                 // 5 ticks × 20ms = 100ms 超时

void K230_Parse_Byte(uint8_t byte) 
{
    static uint8_t state = 0; 
    
    switch(state) 
    {
        case 0:
            if (byte == 0xAA) state = 1; 
            else state = 0;
            break;
        case 1:
            if (byte == 0x55) state = 2; 
            else state = 0;
            break;
        case 2:
            line_sensor_data = byte;
            k230_data_valid = 1;       // Issue 02: 帧新鲜度置位
            k230_timeout_cnt = 0;      // Issue 02: 归零超时计数
            state = 0;
            break;
        default:
            state = 0;
            break;
    }
}

int16_t K230_Get_Turn_Speed(uint8_t sensor_val)
{
    static int16_t last_turn = 0; // 记忆上一次转向值，0x00 时保持方向惯性
    int16_t turn_speed = 0;

    switch(sensor_val)
    {
        case 0x0C: turn_speed = 0;   break;    // 001100 正中

        // 微偏 (保守: ±2~4，避免直线段振荡)
        case 0x08: turn_speed = 2;   break;    // 001000
        case 0x04: turn_speed = -2;  break;    // 000100
        case 0x18: turn_speed = 4;   break;    // 011000
        case 0x06: turn_speed = -4;  break;    // 000110

        // 中度偏 (保守: ±6~7)
        case 0x1C: turn_speed = 6;   break;    // 011100
        case 0x0E: turn_speed = -6;  break;    // 001110
        case 0x38: turn_speed = 7;   break;    // 111000
        case 0x07: turn_speed = -7;  break;    // 000111

        // 重度偏（单侧内路）
        case 0x10: turn_speed = 10;  break;    // 010000
        case 0x02: turn_speed = -10; break;    // 000010

        // 极偏最外侧 (取 H0 与第四问完成中间值: ±16~20)
        case 0x30: turn_speed = 16;  break;    // 110000
        case 0x20: turn_speed = 20;  break;    // 100000
        case 0x03: turn_speed = -16; break;    // 000011
        case 0x01: turn_speed = -20; break;    // 000001

        // 全白脱线 — 保持上次方向惯性（防御性编程，实际在 main.c 双模切换中被拦截）
        case 0x00:
            turn_speed = last_turn;
            break;

        // 未识别状态 — 故障安全原则：不修正=安全，比错误转向好
        default:
            turn_speed = 0;
            break;
    }

    last_turn = turn_speed;
    return turn_speed;
}

// ---- Issue 02: K230 数据新鲜度管理 ----
// 每 TIM6 tick (20ms) 递增超时计数，超时后清零 valid
void K230_Timeout_Tick(void) {
    if (!k230_data_valid) return;     // 已经超时，无需再计数
    k230_timeout_cnt++;
    if (k230_timeout_cnt >= K230_TIMEOUT_TICKS) {
        k230_data_valid = 0;
        k230_timeout_cnt = 0;
    }
}

void K230_Timeout_Reset(void) {
    k230_timeout_cnt = 0;
    k230_data_valid = 0;   // Start 时强制从新鲜帧开始
}