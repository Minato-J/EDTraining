#include "task.h"
#include "main.h"      
#include "stdio.h"
#include "wit_imu.h"   
#include "k230_track.h" 

extern float target_angle;
extern int16_t base_speed;
extern float Yaw_Offset;
extern float current_angle;

// ---- Task 2 & 3 参数 ----
#define BASE_SPEED_STRAIGHT   60
#define BASE_SPEED_CURVE      40
#define RIGHT_HALF_CIRCLE  (-185.0f)  // 右半圆目标角（B→C，顺时针）
#define LEFT_HALF_CIRCLE   (+185.0f)  // 左半圆目标角（D→A，逆时针）
#define YAW_THRESHOLD         8.0f    // 退出阈值（度）

// ---- Task 3 参数 ----
#define DIAG_AC_ANGLE   (-45.0f)   // A→C 对角线方向（左上→右下）
#define DIAG_BD_ANGLE   (-135.0f)  // B→D 对角线方向（右上→左下）

uint16_t selected_task = 1;
uint16_t task_running = 0;
uint8_t current_state = 0;
uint8_t last_line_status = 0x00;
uint8_t count = 0;            
uint8_t lap_count = 0; // 任务4 圈数计数

// ---- 节点计数去抖 ----
#define DEBOUNCE_INIT 10 // 去抖窗口 10 * 20ms = 200ms
uint8_t count_debounce = 0;

// ---- 前馈差速 ----
int16_t ff_diff = 0; // 左右轮前馈差速

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
            count_debounce = 0;
            lap_count = 0;
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

//
static void Run_Task_3(void)
{
    if (count == 0)
    {
        // A→C: 对角线直线段（左上→右下）
        target_angle = DIAG_AC_ANGLE;
        base_speed = BASE_SPEED_STRAIGHT;
        ff_diff = 0;
    }
    else if (count == 1)
    {
        // C→B: 右半圆弧（顺时针）
        target_angle = RIGHT_HALF_CIRCLE;
        base_speed = BASE_SPEED_CURVE;
        ff_diff = 10;
    }
    else if (count == 2)
    {
        // B→D: 对角线直线段（右上→左下）
        target_angle = DIAG_BD_ANGLE;
        base_speed = BASE_SPEED_STRAIGHT;
        ff_diff = 0;
    }
    else if (count == 3)
    {
        // D→A: 左半圆弧（逆时针）
        target_angle = LEFT_HALF_CIRCLE;
        base_speed = BASE_SPEED_CURVE;
        ff_diff = -10;
    }
    else if (count >= 4)
    {
        // 回到 A，停车
        task_running = 0;
        base_speed = 0;
        ff_diff = 0;
    }
}


//
static void Run_Task_4(void)
{
    if (count >= 4)
    {
        lap_count++;
        if (lap_count >= 4)
        {
            // 跑完 4 圈，停车
            task_running = 0;
            base_speed = 0;
            ff_diff = 0;
            return;
        }
        else
        {
            // 没跑完，进入下一圈
            count = 0;
        }
    }

    if (count == 0)
    {
        target_angle = DIAG_AC_ANGLE;
        base_speed = BASE_SPEED_STRAIGHT;
        ff_diff = 0;
    }
    else if (count == 1)
    {
        target_angle = RIGHT_HALF_CIRCLE;
        base_speed = BASE_SPEED_CURVE;
        ff_diff = 10;
    }
    else if (count == 2)
    {
        target_angle = DIAG_BD_ANGLE;
        base_speed = BASE_SPEED_STRAIGHT;
        ff_diff = 0;
    }
    else if (count == 3)
    {
        target_angle = LEFT_HALF_CIRCLE;
        base_speed = BASE_SPEED_CURVE;
        ff_diff = -10;
    }
}

//
static void Run_Task_2(void)
{
    switch (count)
    {
        case 0:
            // A→B 直线段，保持初始朝向
            target_angle = 0.0f;
            base_speed = BASE_SPEED_STRAIGHT;
            ff_diff = 0;
            break;

        case 1:
            // B→C 右半圆弧，目标角度 -185°
            target_angle = RIGHT_HALF_CIRCLE;
            base_speed = BASE_SPEED_CURVE;
            ff_diff = 10; // 差速前馈：右转，左轮快，右轮慢
            break;

        case 2:
            // C→D 直线段，朝向 180°
            target_angle = 180.0f;
            base_speed = BASE_SPEED_STRAIGHT;
            ff_diff = 0;
            break;

        case 3:
            // D→A 左半圆弧，目标角度 +185°
            target_angle = LEFT_HALF_CIRCLE;
            base_speed = BASE_SPEED_CURVE;
            ff_diff = -10; // 差速前馈：左转，右轮快，左轮慢
            break;

        case 4:
        default:
            // 回到 A，停车
            task_running = 0;
            base_speed = 0;
            ff_diff = 0;
            break;
    }
}

void Task_Dispatcher(void)
{
    if (task_running == 1) 
    {
        switch(selected_task) 
        {
            case 1: Run_Task_1(); break; 
            case 2: Run_Task_2(); break; 
            case 3: Run_Task_3(); break;
            case 4: Run_Task_4(); break;
        }
    }
    else 
    {
        base_speed = 0; // �p�G?�b�]��?�A��?��?�t��
    }
}