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
    static int16_t last_turn = 0; // ??�W�@����?�V�A???�ϩR��
    int16_t turn_speed = 0;

    switch(sensor_val) 
    {
			// === �����~���A���t���� ===
  case 0x0C: turn_speed = 0;    break;     // ��38�� - ��?�A���Χ�
  case 0x08: turn_speed = 5;    break;    // ��41�� - �L����
  case 0x04: turn_speed = -5;   break;    // ��42�� - �L�k��
  case 0x18: turn_speed = 10;   break;    // ��45�� - ������
  case 0x06: turn_speed = -10;  break;    // ��46�� - ���k��
  case 0x10: turn_speed = 15;   break;    // ��47�� - �j����
  case 0x02: turn_speed = -15;  break;    // ��48�� - �j�k��
  case 0x30: turn_speed = 30;   break;    // ��51�� - ?������
  case 0x20: turn_speed = 40;   break;    // ��52�� - ��ݥ���
  case 0x03: turn_speed = -30;  break;    // ��53�� - ?���k��
  case 0x01: turn_speed = -40;  break;    // ��54�� - ��ݥk��
  case 0x3F: turn_speed = 0;    break;
//        // === �����~���A���t���� ===
//        case 0x0C: turn_speed = 0;  break;   // 001100

//        // === �L�L���� (?����V?) ===
//        case 0x08: turn_speed = 5; break;  // 001000
//        case 0x04: turn_speed = -5;  break;  // 000100
//        
//        // === ���װ��� (�y�L�ΤO����V?) ===
//        case 0x18: turn_speed = 15; break;  // 011000
//        case 0x06: turn_speed = 20;  break;  // 000110
//        case 0x10: turn_speed = 15; break;  // 010000 
//        case 0x02: turn_speed = 20;  break;  // 000010 

//        // === ?�����áA��?�X?�D�F (�r����V?) ===
//        case 0x30: turn_speed = -25; break;  // 110000 
//        case 0x20: turn_speed = -30; break;  // 100000 
//        case 0x03: turn_speed = 25;  break;  // 000011 
//        case 0x01: turn_speed = 30;  break;  // 000001 

//        case 0x00: 
//            if (last_turn > 0) turn_speed = 70; 
//            else if (last_turn < 0) turn_speed = -70;
//            else turn_speed = 0;
//            break;
        // === �Q�r���f (���� 0x3F) ===
          

     default: 
        turn_speed = last_turn; 
        break;
    }
    
    last_turn = turn_speed; 
    return turn_speed;
}