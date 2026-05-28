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

//
static void Run_Task_3(void)
{
    static uint16_t wait_tick = 0;

    // 统一的角度误差计算
    float err = target_angle - current_angle;
    while (err > 180.0f)  err -= 360.0f;
    while (err < -180.0f) err += 360.0f;
    if (err < 0) err = -err;

    // 弃用 count 做状态机，改用发车会自动清零的 current_state
    switch (current_state)
    {
        // ============================================================
        // 状态 0: A点原地转动对齐角度 (-50.2度)
        // ============================================================
        case 0:
            base_speed = 0;         // 原地不动
            target_angle = -50.2f;  // A→C 精确对角线夹角

            if (err < 3.0f) 
            {
                wait_tick++;
                if (wait_tick >= 20) // 稳定保持约 200ms
                {
                    count = 0;       // 出发前清空计数
                    current_state = 1; // 切换到状态 1：直线冲锋
                    wait_tick = 0;
                }
            }
            else { wait_tick = 0; }
            break;

        // ============================================================
        // 状态 1: 直线全速冲向 C 点，等待撞线
        // ============================================================
        case 1:
            target_angle = -50.2f;  
            base_speed = 60;         

            // 当小车在 TIM6 中撞击 C 点黑线导致 count 变成 1 时，切入下一状态
            if (count >= 1) 
            {
                current_state = 2;   // 切换到状态 2：C点回正
                wait_tick = 0;
            }
            break;

        // ============================================================
        // 状态 2: C点原地回正 (强制锁死 count = 1，防止晃动误触发)
        // ============================================================
        case 2:
            base_speed = 0;         // 原地不动
            target_angle = 0.0f;    // 角度回正
            count = 1;              // 【核心】强行把 count 锁死在 1，无视任何传感器抖动

            if (err < 3.0f) 
            {
                wait_tick++;
                if (wait_tick >= 30) // 稳定保持约 300ms
                {
                    count = 1;       // 确保出发前 count 为 1
                    current_state = 3; // 切换到状态 3：解锁并循迹
                    wait_tick = 0;
                }
            }
            else { wait_tick = 0; }
            break;

        // ============================================================
        // 状态 3: 沿半圆弧循迹行驶，直到再次触线/离线让 count 变成 2
        // ============================================================
        case 3:
            base_speed = 30;        // 赋予循迹基础速度
            // 此时由于底层 line_sensor_data 不为 0x00，会自动进入摄像头循迹模式
            
            if (count >= 2) 
            {
                current_state = 4;   // 看到下一个点，去停车
            }
            break;

        // ============================================================
        // 状态 4: 任务结束，停车
        // ============================================================
        case 4:
        default:
            base_speed = 0;
            task_running = 0;       // 关闭运行标志
            current_state = 0;      // 复位状态
            count = 0;              // 复位计数
            wait_tick = 0;
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