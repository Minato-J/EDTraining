/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "Motor.h"
#include "Encoder.h"
#include "pid.h"
#include "stdio.h"
#include "wit_imu.h"
#include <math.h>
#include "k230_track.h"
#include "task.h"
#include "oled.h"
#include "yaw_track.h"
#include "reach_point.h"   // Issue 03: 声光指示
#include "car_control.h"  // Issue 06: 显式控制模式
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
// ==== Uart2 & Uart3 ===
uint8_t rx_data;

// ==== Pid para === 
PID_TypeDef pid_left;
PID_TypeDef pid_right;  
#ifdef PID_USE_VELOCITY_FORM
	PID_Velocity_TypeDef pid_vel_left;
	PID_Velocity_TypeDef pid_vel_right;
#endif

// ====  pid ==== 
int16_t target_v_left = 0;  // 左轮独立目标
int16_t target_v_right = 0; // 右轮独立目标
int16_t current_v_left = 0;
int16_t current_v_right = 0; 

//=== 角度环 ===
PID_TypeDef pid_angle;
float Yaw_Offset = 0;
float current_angle = 0;
uint8_t angle_initialized = 0;   // 角度过滤器哨兵—任务启动时由 task.c 清零
extern uint8_t lap_count;

#ifdef DEBUG_UART               // Issue 04: UART1 调试输出
	static char debug_buf[80];
	static volatile uint8_t debug_tick_cnt = 0;
	#define DEBUG_TICK_INTERVAL 5
#endif

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  MX_USART1_UART_Init();
  MX_TIM6_Init();
  MX_USART2_UART_Init();
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */
    //HAL_Delay(500);  // == 角度校准
	Encoder_Init();
	Motor_Init();
	//Motor_SetSpeed_B(-10);
	//Motor_SetSpeed_A(-10);
	OLED_Init();
	OLED_Clear();
	OLED_ShowString(0, 0, (uint8_t *)"Task:", 16, 1);
	OLED_ShowString(0, 16, (uint8_t *)"startflag:", 16, 1);
	OLED_ShowString(0, 32, (uint8_t *)"count:", 16, 1);
	OLED_Refresh();
	PID_Init(&pid_left,0.9f, 0.12f, 0.0f, 500.0f);  // == lelf
	PID_Init(&pid_right, 0.8f, 0.12f, 0.0f, 500.0f);  // == right
	PID_Init(&pid_angle, 1.2f, 0.0f, 0.6f, 40.0f); // == angle
#ifdef PID_USE_VELOCITY_FORM
		PID_Velocity_Init(&pid_vel_left, 15.0f, 3.0f, 1000);
		PID_Velocity_Init(&pid_vel_right, 15.0f, 3.0f, 1000);
#endif
		ReachPoint_Init();
	Car_Init(&pid_angle);                // Issue 06: 显式控制模式初始化
	Task_Manager_Init();
	HAL_UART_Receive_IT(&huart2, &rx_data, 1);    
    HAL_UART_Receive_IT(&huart3, &k230_rx_data, 1);
    HAL_TIM_Base_Start_IT(&htim6);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  // == 串口打印数据进行测试
	               //printf("%.2f,%d,%d\n", current_angle, current_v_left, current_v_right);
				  //printf("%.2f\n",IMU_Data.Yaw);
					//HAL_Delay(10)；
	   //printf("xrf%.2f,%.2f\n",IMU_Data.Yaw,current_angle);
	  // HAL_Delay(50);
	  // == 
	   // 1. 扫描按键，决定任务的切换与启动 (实现在 task.c 中)
      Task_Key_Scan();
      // 2. 任务分发，跑对应的状态机 (实现在 task.c 中)
      Task_Dispatcher();
	  //
	  OLED_ShowSignedNum(48, 0,selected_task, 4, 16, 1);
	  OLED_ShowSignedNum(80, 16, task_running, 4, 16, 1);
	  OLED_ShowSignedNum(48, 32, count, 4, 16, 1);
	  OLED_Refresh();
	  OLED_ShowSignedNum(48, 48, lap_count, 4, 16, 1);
	  OLED_ShowSignedNum(48, 56, (int16_t)YawTrack_GetCumulative(), 4, 16, 1);

#ifdef DEBUG_UART
	      if (debug_tick_cnt >= DEBUG_TICK_INTERVAL) {
	          debug_tick_cnt = 0;
		          int len = sprintf(debug_buf, "%d,%d,%.1f,%.1f,%d,%.1f
",
	              current_v_left, current_v_right,
	              current_angle, Car_GetTargetAngle(),
	              line_sensor_data, Car_GetTurnOutSmooth());
	          HAL_UART_Transmit(&huart1, (uint8_t*)debug_buf, len, 10);
	      }
#endif

	  //printf("%d\r\n",selected_task);
      HAL_Delay(10);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
// ControlState_Reset: 任务启动时清零 ISR 的累积状态，防止上次运行残留
// P1#3: blind_ticks 归零 → 下次 Start 不会从旧值续计
// P2#5: turn_out_smooth 归零 → 新任务首帧差速不受旧转弯量污染
// P2#8: PID 状态重置 → 积分/上次误差/输出归零
void ControlState_Reset(void) {
    Car_ResetState();     // Issue 06: 清零 blind_ticks + turn_out_smooth（已移入 car_control）
    PID_Init(&pid_left, 0.9f, 0.12f, 0.0f, 500.0f);
    PID_Init(&pid_right, 0.8f, 0.12f, 0.0f, 500.0f);
    PID_Init(&pid_angle, 1.2f, 0.0f, 0.6f, 40.0f);
#ifdef PID_USE_VELOCITY_FORM
		PID_Velocity_Init(&pid_vel_left, 15.0f, 3.0f, 1000);
		PID_Velocity_Init(&pid_vel_right, 15.0f, 3.0f, 1000);
#endif

}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	// 判断是不是我们用来做 PID 的 TIM6
    if (htim->Instance == TIM6) 
    {
		// == 中断测试 == 
		//printf("test");
		// == 判断计数 == 
		if (task_running == 1)
        {
            if (count_debounce > 0)
            {
                count_debounce--;
            }

            uint8_t current_line = (line_sensor_data != 0x00);
            if (last_line_status == 1 && current_line == 0)
            {
                if (count_debounce == 0)
                {
                    count++;
                    count_debounce = 10; // DEBOUNCE_INIT (200ms)
                }
            }
            last_line_status = current_line;
	        K230_Timeout_Tick();   // Issue 02: K230 超时检测
	        ReachPoint_Tick();     // Issue 03: 声光自动恢复
        }
        // ================= 1. 数据清洗：角度突变过滤器 + NaN 哨兵 =================
        static float last_valid_angle = 0;

        // 减去 Yaw_Offset，让发车时按下按键后的零点生效！
        float raw_yaw = IMU_Data.Yaw - Yaw_Offset;

        // NaN 入口哨兵: 一旦进入 current_angle 永久污染 YawTrack + PID integral
        // x != x 是 IEEE 754 惯用法，NaN 是唯一不满足自反性的值
        if (raw_yaw != raw_yaw) goto skip_angle_update;
        float diff = raw_yaw - last_valid_angle;
        while (diff > 180.0f)  diff -= 360.0f;
        while (diff < -180.0f) diff += 360.0f;

        if (!angle_initialized) {
            // │ 首个有效读数：无条件接受，建立基线
            current_angle = raw_yaw;
            last_valid_angle = raw_yaw;
            angle_initialized = 1;
        } else if (fabsf(diff) > 30.0f) {
            // │ 突变尖峰：丢弃，保持上次有效值
            current_angle = last_valid_angle;
        } else {
            // │ 正常读数：接受并更新
            current_angle = raw_yaw;
            last_valid_angle = raw_yaw;
        }
        // yaw_cumulative 追踪：每帧累加航向变化量
        YawTrack_Update(current_angle);
        skip_angle_update:   // NaN 哨兵跳转点：跳过本帧角度更新但继续执行控制
        // ================= 2. 核心大脑：Car_ControlLoop 统一调度 (Issue 06) =================
        // 替换原 ~50 行双模切换 (rotating/blind/line) + turn_out 平滑 + 差速合成
        Car_SensorInputs in = {current_angle, line_sensor_data, k230_data_valid};
        Car_ControlOutputs out = Car_ControlLoop(in);

        target_v_left  = out.target_v_left;
        target_v_right = out.target_v_right;
        if (out.emergency_stop) task_running = 0;
        // ================= 3. 底层肌肉：速度环 PID =================
        // === 左轮 ===
        int16_t raw_l = Encoder_Get_Count_Left();
        static float fil_l = 0; 
        fil_l = 0.7f * fil_l + 0.3f * (float)raw_l; 
#ifdef PID_USE_VELOCITY_FORM
		float out_l = PID_Velocity_Compute(&pid_vel_left, target_v_left, (int)fil_l);
#else
        float out_l = PID_Compute(&pid_left, (float)target_v_left, fil_l);
#endif
        Motor_SetSpeed_A((int16_t)out_l);
        current_v_left = (int16_t)fil_l;

        // === 右轮 ===
        int16_t raw_r = Encoder_Get_Count_Right();
        static float fil_r = 0; 
        fil_r = 0.7f * fil_r + 0.3f * (float)raw_r; 
#ifdef PID_USE_VELOCITY_FORM
		float out_r = PID_Velocity_Compute(&pid_vel_right, target_v_right, (int)fil_r);
#else
        float out_r = PID_Compute(&pid_right, (float)target_v_right, fil_r);
#endif
        Motor_SetSpeed_B((int16_t)out_r);
        current_v_right = (int16_t)fil_r;
#ifdef DEBUG_UART
			debug_tick_cnt++;                 // Issue 04: 调试计数
#endif
		//printf("%d,%d\r\n",raw_l,raw_r);
    }
}

//uart
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if(huart->Instance == USART2) 
    {
        WIT_Parse_Byte(rx_data);
        HAL_UART_Receive_IT(&huart2, &rx_data, 1); 
    }
    
    if(huart->Instance == USART3) 
    {
        // 把收到的 1 个字节交给封装好的状态机
        K230_Parse_Byte(k230_rx_data);
        // 重新开启串口 3 接收中断
        HAL_UART_Receive_IT(&huart3, &k230_rx_data, 1); 
		//
    }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
