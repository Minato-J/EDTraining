#include "task.h"
#include "main.h"      
#include "stdio.h"
#include "wit_imu.h"   
#include "k230_track.h" 

extern float target_angle;
extern int16_t base_speed;
extern float Yaw_Offset;
extern float current_angle;

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
static void Run_Task_3(void)
{
    static uint16_t wait_tick = 0;
switch (count)
    {
        // ============================================================
        // count 0: A点原地转动对齐角度 -> 完毕后直线行驶冲向C点
        // ============================================================
        case 0:
            if (current_state == 0) 
            {
                // ---- 【阶段 1：原地旋转】 ----
                base_speed = 0;         // 速度为 0，使底层 PID 仅进行原地转向
                target_angle = -38.2f;  // A→C 的精确对角线夹角 (-arctan(120/100))

                // 计算当前角度与目标角度的绝对误差
                float err = target_angle - current_angle;
                while (err > 180.0f)  err -= 360.0f;
                while (err < -180.0f) err += 360.0f;
                if (err < 0) err = -err;

                // 角度误差小于 3 度时，认为角度已对准
                if (err < 3.0f) 
                {
                    wait_tick++;
                    if (wait_tick >= 10) // 稳定保持大约 200ms
                    {
                        current_state = 1; // 切换到阶段 2：直线行驶
                        wait_tick = 0;
                    }
                }
                else 
                {
                    wait_tick = 0; // 角度未对准时清空计数
                }
            }
            else if (current_state == 1) 
            {
                // ---- 【阶段 2：直线行驶】 ----
                target_angle = -38.2f;  // 锁死目标角度
                base_speed = 60;         // 赋予前进速度，全速冲向 C 点
                
                // 注：此阶段车往前走，直到触发黑线使 TIM6 中断将 count 自增为 1
            }
            break;

        // ============================================================
        // count 1: 到达 C 点，任务停止
        // ============================================================
        case 1:
        default:
            base_speed = 0;      // 电机停转
            task_running = 0;    // 关闭任务运行状态
            current_state = 0;   // 复位子状态计数器，供下次发车使用
            wait_tick = 0;       // 复位时间计数器
            break;
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
            case 1: Run_Task_1(); break; 
            case 2: Run_Task_2(); break; 
            case 3: Run_Task_3(); break;
//            case 4: Run_Task_4();  break;
        }
    }
    else 
    {
        base_speed = 0; // �p�G?�b�]��?�A��?��?�t��
    }
}