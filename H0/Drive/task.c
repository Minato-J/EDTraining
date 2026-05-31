/*
 * 任务-赛题映射表（基于第四问完成代码，重编号后合并到 H0）
 *
 * Task 1 → 赛题(1) A→B 直线循迹（基本要求1）
 *          来源: 原第四问完成 Run_Task_4（count 驱动），重新启用
 * Task 2 → 赛题(2) A→B→C→D→A 外圈循迹（基本要求2）
 *          来源: 原第四问完成 Run_Task_3（count 驱动），重新启用，弧段角度已修复
 *          H0 增强: ff_diff 前馈差速
 * Task 3 → 赛题(3) A→C→B→D→A 对角线循迹，1圈（发挥）
 *          来源: 原第四问完成 Run_Task_2（9状态航点单圈），实测已跑通
 * Task 4 → 赛题(4) A→C→B→D→A 三圈，不停车（发挥）
 *          来源: 原第四问完成 Run_Task_1（29状态航点三圈），中途wait_tick停车已注释
 * Task 5 → 扩展: A→C→B→D→A ×4圈（保留，不入赛题）
 *          来源: H0 原 Run_Task_4（count 驱动4圈），保留供调试
 *
 * H0 基础设施: count_debounce 节点去抖 + ff_diff 前馈差速
 *
 * 导航策略:
 *   Task 1/2: count 驱动连续循迹（K230 + 角度保持 + ff_diff）
 *   Task 3:   航点到航点，每节点停车→IMU旋转→冲刺
 *   Task 4:   航点到航点但不等待（停车代码注释），连续三圈
 *   Task 5:   count 驱动4圈连续循迹（保留）
 */

#include "task.h"
#include "main.h"
#include "stdio.h"
#include "wit_imu.h"
#include "k230_track.h"
#include "math.h"

extern float target_angle;
extern int16_t base_speed;
extern float Yaw_Offset;
extern float current_angle;

// ---- 通用参数 ----
#define BASE_SPEED_STRAIGHT   60
#define BASE_SPEED_CURVE      30
// 以下宏供 Task 2/5 的 count 驱动弧段使用
#define RIGHT_HALF_CIRCLE  (-185.0f)
#define LEFT_HALF_CIRCLE   (+185.0f)
#define YAW_THRESHOLD         8.0f
// 对角线角度（供 Task 5 使用）
#define DIAG_AC_ANGLE   (-45.0f)
#define DIAG_BD_ANGLE   (-135.0f)

// ---- 节点计数去抖 (H0 基础设施) ----
#define DEBOUNCE_INIT 10 // 去抖窗口 10 * 20ms = 200ms
uint8_t count_debounce = 0;

// ---- 前馈差速 (H0 基础设施) ----
int16_t ff_diff = 0;

uint8_t lap_count = 0;
uint8_t rotating = 0;              // 1=正在原地旋转，屏蔽 K230
uint16_t selected_task = 1;
uint16_t task_running = 0;
uint8_t current_state = 0;
uint8_t last_line_status = 0x00;
uint8_t count = 0;
static uint16_t wait_tick = 0;

void Task_Manager_Init(void) {
    selected_task = 1;
    task_running = 0;
    current_state = 0;
    last_line_status = 0x00;
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
            if (selected_task > 5) selected_task = 1;
            // 比赛当天改上限为 4，避免误选 Task 5
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

// ================================================================
// Task 1: 赛题(1) A→B 直线
// ================================================================
static void Run_Task_1(void)
{
    if (count == 0)
    {
        target_angle = 0.0f;
        base_speed = BASE_SPEED_STRAIGHT;
        ff_diff = 0;
    }
    else if (count == 1)
    {
        task_running = 0;
        base_speed = 0;
    }
}

// ================================================================
// Task 2: 赛题(2) A→B→C→D→A 外圈（count 驱动 + ff_diff）
// ================================================================
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
            // B→C 右半圆弧
            target_angle = RIGHT_HALF_CIRCLE;
            base_speed = BASE_SPEED_CURVE;
            ff_diff = 10;
            break;

        case 2:
            // C→D 直线段，朝向 180°
            target_angle = 180.0f;
            base_speed = BASE_SPEED_STRAIGHT;
            ff_diff = 0;
            break;

        case 3:
            // D→A 左半圆弧
            target_angle = LEFT_HALF_CIRCLE;
            base_speed = BASE_SPEED_CURVE;
            ff_diff = -10;
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

// ================================================================
// Task 3: 赛题(3) A→C→B→D→A 1圈（9状态航点，来源: 第四问完成原 Run_Task_2）
// ================================================================
static void Run_Task_3(void)
{
    switch(current_state)
    {
        // state 0: A点原地旋转 -> 目标 -38度
        case 0:
            rotating = 1;
            base_speed = 0;
            target_angle = -38.0f;
            ff_diff = 0;

            wait_tick++;
            if (wait_tick >= 10)    // 700ms
            {
                count = 0;
                current_state = 1;
                wait_tick = 0;
                rotating = 0;
            }
            break;

        // state 1: 保持对角推进 -> 等待触发 C 点
        case 1:
            target_angle = -38.0f;
            base_speed = 60;
            ff_diff = 0;

            if (count >= 1)
            {
                base_speed = 0;
                current_state = 2;
            }
            break;

        // state 2: C点原地扭头 -> 目标 0度
        case 2:
            rotating = 1;
            base_speed = 0;
            target_angle = 0.0f;
            count = 1;

            wait_tick++;
            if (wait_tick >= 10)
            {
                count = 1;
                current_state = 3;
                wait_tick = 0;
                rotating = 0;
            }
            break;

        // state 3: 保持 0 度推进 -> 等待 B 点
        case 3:
            base_speed = 30;
            target_angle = 0.0f;
            ff_diff = 0;

            if (count >= 2)
            {
                base_speed = 0;
                current_state = 4;
            }
            break;

        // state 4: B点原地大调头 -> 目标 -144度
        case 4:
            rotating = 1;
            base_speed = 0;
            target_angle = -144.0f;

            wait_tick++;
            if (wait_tick >= 10)
            {
                count = 2;
                current_state = 5;
                wait_tick = 0;
                rotating = 0;
            }
            break;

        // state 5: 保持对角返回 -> 等待 D 点
        case 5:
            base_speed = 60;
            ff_diff = 0;

            if (count >= 3)
            {
                base_speed = 0;
                current_state = 6;
            }
            break;

        // state 6: D点原地回正 -> 目标 180度
        case 6:
            rotating = 1;
            base_speed = 0;
            target_angle = 180.0f;
            wait_tick++;
            if (wait_tick >= 10)
            {
                count = 3;
                current_state = 7;
                wait_tick = 0;
                rotating = 0;
            }
            break;

        // state 7: 直行冲向 A 点
        case 7:
            base_speed = 30;
            ff_diff = 0;

            if (count >= 4)
            {
                current_state = 8;
            }
            break;

        // state 8 / 终点停车
        case 8:
        default:
            base_speed = 0;
            task_running = 0;
            current_state = 0;
            count = 0;
            wait_tick = 0;
            ff_diff = 0;
            break;
    }
}

// ================================================================
// Task 4: 赛题(4) A→C→B→D→A 三圈不停车
//         来源: 第四问完成原 Run_Task_1（29状态航点），中途wait_tick停车已注释
// ================================================================
static void Run_Task_4(void)
{
    switch(current_state)
    {
        // ========================================================
        // 【第一圈】 状态 0 ~ 7
        // ========================================================

        // 状态 0: 第一圈 - A点起步 -> 目标 -38度 (不停车)
        case 0:
            base_speed = 60;
            target_angle = -38.0f;
            ff_diff = 0;
            count = 0;
            current_state = 1;
            break;

        // 状态 1: 第一圈 - 保持对角推进 -> 等待触发 C 点
        case 1:
            target_angle = -38.0f;
            base_speed = 60;
            ff_diff = 0;

            if (count >= 1)
            {
                base_speed = 0;
                current_state = 2;
            }
            break;

        // 状态 2: 第一圈 - C点换方向 -> 目标 0度 (不停车)
        case 2:
            base_speed = 30;
            target_angle = 0.0f;
            count = 1;
            current_state = 3;
            break;

        // 状态 3: 第一圈 - 保持 0 度推进 -> 等待 B 点
        case 3:
            base_speed = 30;
            target_angle = 0.0f;
            ff_diff = 0;

            if (count >= 2)
            {
                base_speed = 0;
                current_state = 4;
            }
            break;

        // 状态 4: 第一圈 - B点换方向 -> 目标 -144度 (不停车)
        case 4:
            base_speed = 60;
            target_angle = -144.0f;
            count = 2;
            current_state = 5;
            break;

        // 状态 5: 第一圈 - 保持对角返回 -> 等待 D 点
        case 5:
            base_speed = 60;
            target_angle = -144.0f;
            ff_diff = 0;

            if (count >= 3)
            {
                base_speed = 0;
                current_state = 6;
            }
            break;

        // 状态 6: 第一圈 - D点换方向 -> 目标 180度 (不停车)
        case 6:
            base_speed = 30;
            target_angle = 180.0f;
            count = 3;
            current_state = 7;
            break;

        // 状态 7: 第一圈 - 直行冲向 A 点 -> 触发后切入第二圈
        case 7:
            base_speed = 30;
            target_angle = 180.0f;
            ff_diff = 0;

            if (count >= 4)
            {
                base_speed = 0;
                count = 0;
                current_state = 10;
                wait_tick = 0;
            }
            break;


        // ========================================================
        // 【第二圈】 状态 10 ~ 17
        // ========================================================

        // 状态 10: 第二圈 - A点起步 -> 目标 -38度 (不停车)
        case 10:
            base_speed = 60;
            target_angle = -38.0f;
            count = 0;
            current_state = 11;
            break;

        // 状态 11: 第二圈 - 保持对角推进 -> 等待触发 C 点
        case 11:
            target_angle = -38.0f;
            base_speed = 60;

            if (count >= 1)
            {
                base_speed = 0;
                current_state = 12;
            }
            break;

        // 状态 12: 第二圈 - C点换方向 -> 目标 0度 (不停车)
        case 12:
            base_speed = 30;
            target_angle = 0.0f;
            count = 1;
            current_state = 13;
            break;

        // 状态 13: 第二圈 - 保持 0 度推进 -> 等待 B 点
        case 13:
            base_speed = 30;
            target_angle = 0.0f;

            if (count >= 2)
            {
                base_speed = 0;
                current_state = 14;
            }
            break;

        // 状态 14: 第二圈 - B点换方向 -> 目标 -144度 (不停车)
        case 14:
            base_speed = 60;
            target_angle = -144.0f;
            count = 2;
            current_state = 15;
            break;

        // 状态 15: 第二圈 - 保持对角返回 -> 等待 D 点
        case 15:
            base_speed = 60;
            target_angle = -144.0f;

            if (count >= 3)
            {
                base_speed = 0;
                current_state = 16;
            }
            break;

        // 状态 16: 第二圈 - D点换方向 -> 目标 180度 (不停车)
        case 16:
            base_speed = 30;
            target_angle = 180.0f;
            count = 3;
            current_state = 17;
            break;

        // 状态 17: 第二圈 - 直行冲向 A 点 -> 触发后切入最后一圈
        case 17:
            base_speed = 30;
            target_angle = 180.0f;

            if (count >= 4)
            {
                base_speed = 0;
                count = 0;
                current_state = 20;
                wait_tick = 0;
            }
            break;


        // ========================================================
        // 【第三圈 - 最后一圈】 状态 20 ~ 28
        // ========================================================

        // 状态 20: 第三圈 - A点起步 -> 目标 -38度 (不停车)
        case 20:
            base_speed = 60;
            target_angle = -38.0f;
            count = 0;
            current_state = 21;
            break;

        // 状态 21: 第三圈 - 保持对角推进 -> 等待触发 C 点
        case 21:
            target_angle = -38.0f;
            base_speed = 60;

            if (count >= 1)
            {
                base_speed = 0;
                current_state = 22;
            }
            break;

        // 状态 22: 第三圈 - C点换方向 -> 目标 0度 (不停车)
        case 22:
            base_speed = 30;
            target_angle = 0.0f;
            count = 1;
            current_state = 23;
            break;

        // 状态 23: 第三圈 - 保持 0 度推进 -> 等待 B 点
        case 23:
            base_speed = 30;
            target_angle = 0.0f;

            if (count >= 2)
            {
                base_speed = 0;
                current_state = 24;
            }
            break;

        // 状态 24: 第三圈 - B点换方向 -> 目标 -144度 (不停车)
        case 24:
            base_speed = 60;
            target_angle = -144.0f;
            count = 2;
            current_state = 25;
            break;

        // 状态 25: 第三圈 - 保持对角返回 -> 等待 D 点
        //          (最后一圈 angle 微调为 -147°)
        case 25:
            base_speed = 60;
            target_angle = -147.0f;

            if (count >= 3)
            {
                base_speed = 0;
                current_state = 26;
            }
            break;

        // 状态 26: 第三圈 - D点换方向 -> 目标 180度 (不停车)
        case 26:
            base_speed = 30;
            target_angle = 180.0f;
            count = 3;
            current_state = 27;
            break;

        // 状态 27: 第三圈 - 直行冲向终点 A 点
        case 27:
            base_speed = 30;
            target_angle = 180.0f;

            if (count >= 4)
            {
                base_speed = 0;
                current_state = 28;
            }
            break;

        // 状态 28: 终点安全停车
        case 28:
        default:
            base_speed = 0;
            task_running = 0;
            current_state = 0;
            count = 0;
            wait_tick = 0;
            break;
    }
}

// ================================================================
// Task 5: 扩展 A→C→B→D→A ×4圈（来源: H0 原 Run_Task_4，保留）
// ================================================================
static void Run_Task_5(void)
{
    if (count >= 4)
    {
        lap_count++;
        if (lap_count >= 4)
        {
            task_running = 0;
            base_speed = 0;
            ff_diff = 0;
            lap_count = 0;
            return;
        }
        else
        {
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

void Task_Dispatcher(void)
{
    if (task_running == 1)
    {
        switch(selected_task)
        {
            case 1: Run_Task_1(); break;   // 赛题(1) A→B 直线
            case 2: Run_Task_2(); break;   // 赛题(2) A→B→C→D→A 一圈
            case 3: Run_Task_3(); break;   // 赛题(3) A→C→B→D→A (1圈航点)
            case 4: Run_Task_4(); break;   // 赛题(4) A→C→B→D→A (3圈航点, 不停车)
            case 5: Run_Task_5(); break;   // 扩展 4圈 (保留)
        }
    }
    else
    {
        base_speed = 0;
    }
}
