#include "task.h"
#include "main.h"      
#include "stdio.h"
#include "wit_imu.h"   
#include "k230_track.h" 

extern float target_angle;
extern int16_t base_speed;
extern float Yaw_Offset;

// ---- Task 2 参数 ----
#define BASE_SPEED_STRAIGHT   60
#define BASE_SPEED_CURVE      30
#define RIGHT_HALF_CIRCLE  (-185.0f)  // 右半圆目标角（B→C，顺时针）
#define LEFT_HALF_CIRCLE   (+185.0f)  // 左半圆目标角（D→A，逆时针）
#define YAW_THRESHOLD         8.0f    // 退出阈值（度）

uint16_t selected_task = 1;
uint16_t task_running = 0;
uint8_t current_state = 0;
uint8_t last_line_status = 0x00;
uint8_t count = 0;            

void Task_Manager_Init(void) {
    selected_task = 1;  //  one = 1
    task_running = 0;  // startflag == 0 = stop  1= run
    current_state = 0;  // 
    last_line_status = 0x00; //
}

void Task_Key_Scan(void) 
{
	// === shift
    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0) == GPIO_PIN_RESET) 
    {
        HAL_Delay(20); 
        if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0) == GPIO_PIN_RESET && !task_running) 
        {
            selected_task++;
            if (selected_task > 4) selected_task = 1;
            // printf("Task Selected: %d\r\n", selected_task);
            while(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0) == GPIO_PIN_RESET); 
        }
    }
    // == start == 
    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1) == GPIO_PIN_RESET) 
    {
        HAL_Delay(20); 
        if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1) == GPIO_PIN_RESET && !task_running) 
        {
            Yaw_Offset = IMU_Data.Yaw; 
            current_state = 0;
            last_line_status = 0x00;
            count = 0;
            task_running = 1; 
            while(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1) == GPIO_PIN_RESET);
        }
    }
}
// (1)
static void Run_Task_1(void) 
{
	//
	if (count == 0) 
    {
        // 1. ??�J������??�ӡA�_�B�}�O������
        target_angle = 0.0f; // ��?����??0�]??���U��?��?�AYaw_Offset �w?�M�s�F�^
        base_speed = 60;     // 直线速度
    }
	else if (count == 1) 
	{
		task_running = 0;
	}
}
//

//�]2�^
static void Run_Task_2(void)
{
    if (count == 0)
    {
        // A->B: 直线段，保持初始朝向 0 deg
        target_angle = 0.0f;
        base_speed = BASE_SPEED_STRAIGHT;
    }
    else if (count == 1)
    {
        // B->C: 右半圆弧 (顺时针 185 deg)
        target_angle = RIGHT_HALF_CIRCLE;  // -185
        base_speed = BASE_SPEED_CURVE;     // 32，降速防甩出
    }
    else if (count == 2)
    {
        // C->D: 底部直线段，朝向 ~180 deg
        target_angle = 180.0f;
        base_speed = BASE_SPEED_STRAIGHT;
    }
    else if (count == 3)
    {
        // D->A: 左半圆弧 (逆时针 185 deg)
        target_angle = LEFT_HALF_CIRCLE;   // +185
        base_speed = BASE_SPEED_CURVE;
    }
    else if (count >= 4)
    {
        // 回到 A，停车
        task_running = 0;
        base_speed = 0;
    }
}
//
static void Run_Task_3(void)
{
    switch (count)
    {
        case 0:
            // A→B 直线段，保持初始朝向
            target_angle = 0.0f;
            base_speed = 60;
            break;

        case 1:
            // B→C 右半圆弧，目标角度 -185°
            //target_angle = -185.0f;
            base_speed = 30;
            break;

        case 2:
            // C→D 直线段，朝向 180°
            target_angle = 180.0f;
            base_speed = 60;
            break;

        case 3:
            // D→A 左半圆弧，目标角度 +185°
            //target_angle = 185.0f;
            base_speed = 30;
            break;

        case 4:
        default:
            // 回到 A，停车
            task_running = 0;
            base_speed = 0;
            break;
    }
}

void Task_Dispatcher(void)
{
    if (task_running == 1) 
    {
        switch(selected_task) 
        {
            //case 1: Run_Task_1(); break; 
            //case 2: Run_Task_2(); break; 
            case 3: Run_Task_3(); break;
            //case 4: /* Run_Task_4(); */ break;
        }
    }
    else 
    {
        base_speed = 0; // �p�G?�b�]��?�A��?��?�t��
    }
}