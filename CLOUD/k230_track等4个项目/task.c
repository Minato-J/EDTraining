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
static void Run_Task_3(void) 
{
	//
	if (count == 0) 
    {
        // 1. ??遇到任何黑??志，起步并保持直走
        target_angle = 0.0f; // 目?角度??0（??按下按?瞬?，Yaw_Offset 已?清零了）
        base_speed =60;     // ?定基?速度（??值你需要根据你的?机?速???，比如 30~50）
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
    if (count == 0) 
    {
        // ?段 1：? A ?向 B（?黑?白地）
        target_angle = 0.0f;    // 死?初始角度 0 度
        base_speed = 60;        // ?健速度跑完直?
    }
    else if (count == 1) 
    {
        // ?段 2：在 B ?撞?，正在通? B ?? C 弧?
        // 此?系?由于??到有?，?自?在后台?入【?像?循?模式】，我?只需要?基?速度
        base_speed = 40;        // ??速度稍微降一?，保????滑
    }
    else if (count == 2) 
    {
        // ?段 3：? C ?出了弧?，?入 C ?? D 盲?白地
        // 重?：此?方向相反了，陀螺?必?死? 180 度（或者 -180）直行！
//        target_angle = 180.0f;  
//        base_speed = 45;        
			task_running = 0;
    }
	//
}
//
static void Run_Task_1(void)  
{
	 base_speed = 40;
}
//
//
static void Run_Task_4(void)  
{
	
}

void Task_Dispatcher(void)
{
    if (task_running == 1) 
    {
        switch(selected_task) 
        {
            case 1: Run_Task_1(); break; 
            case 2: Run_Task_2();  break; 
            case 3: Run_Task_3(); break;
            case 4: Run_Task_4();break;
        }
    }
    else 
    {
        base_speed = 0; // 如果?在跑任?，死?基?速度
    }
}