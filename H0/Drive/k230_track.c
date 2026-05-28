#include "k230_track.h"

uint8_t line_sensor_data = 0x00;
uint8_t k230_rx_data = 0;

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
            state = 0;             
            break;
        default:
            state = 0;
            break;
    }
}

int16_t K230_Get_Turn_Speed(uint8_t sensor_val) 
{
    static int16_t last_turn = 0; // ??上一次的?向，???救命用
    int16_t turn_speed = 0;

    switch(sensor_val) 
    {
        // === 完美居中，全速直行 ===
        case 0x0C: turn_speed = 0;  break;   // 001100

        // === 微微偏离 (?打方向?) ===
        case 0x08: turn_speed = -15; break;  // 001000
        case 0x04: turn_speed = 15;  break;  // 000100
        
        // === 中度偏离 (稍微用力打方向?) ===
        case 0x18: turn_speed = -25; break;  // 011000
        case 0x06: turn_speed = 25;  break;  // 000110
        case 0x10: turn_speed = -35; break;  // 010000 
        case 0x02: turn_speed = 35;  break;  // 000010 

        // === ?重偏离，快?出?道了 (猛打方向?) ===
        case 0x30: turn_speed = -50; break;  // 110000 
        case 0x20: turn_speed = -60; break;  // 100000 
        case 0x03: turn_speed = 50;  break;  // 000011 
        case 0x01: turn_speed = 60;  break;  // 000001 

        case 0x00: 
            if (last_turn > 0) turn_speed = 70; 
            else if (last_turn < 0) turn_speed = -70;
            else turn_speed = 0;
            break;
        // === 十字路口 (全黑 0x3F) ===
        case 0x3F: turn_speed = 0; break;    

        default: 
            turn_speed = last_turn; 
            break;
    }
    
    last_turn = turn_speed; 
    return turn_speed;
}