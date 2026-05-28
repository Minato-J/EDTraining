#include "task.h"
#include "main.h"      
#include "stdio.h"
#include "wit_imu.h"   
#include "k230_track.h" 

extern float target_angle;
extern int16_t base_speed;
extern float Yaw_Offset;

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
        // 1. ??遇到任何黑??志，起步并保持直走
        target_angle = 0.0f; // 目?角度??0（??按下按?瞬?，Yaw_Offset 已?清零了）
        base_speed = 0;     // ?定基?速度（??值你需要根据你的?机?速???，比如 30~50）
    }
	else if (count == 1) 
	{
		task_running = 0;
	}
}
//

//（2）
static void Run_Task_2(void)  
{
	//
        if (count == 1) 
        {
            base_speed = 20;       
        }
        else if (count == 2) 
        {
            target_angle = 180.0f; 
            base_speed = 50; 
        }
        else if (count == 3) 
        {
            base_speed = 40;   
        }
        else if (count >= 4) 
        {
            task_running = 0;  
            base_speed = 0;
        }
	//
}
//
static void Run_Task_3(void)  
{
	
}

void Task_Dispatcher(void)
{
    if (task_running == 1) 
    {
        switch(selected_task) 
        {
            case 1: Run_Task_1(); break; 
            case 2: /*Run_Task_2();  */   break; 
            case 3: /* Run_Task_3(); */ break;
            case 4: /* Run_Task_4(); */ break;
        }
    }
    else 
    {
        base_speed = 0; // 如果?在跑任?，死?基?速度
    }
}