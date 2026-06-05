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
#include "yaw_track.h"

extern float target_angle;
extern int16_t base_speed;
extern float Yaw_Offset;
extern float current_angle;
extern uint8_t angle_initialized;    // main.c 中角度过滤器哨兵 (P0#2 fix)

// ---- 通用参数 ----
#define BASE_SPEED_STRAIGHT   60
#define BASE_SPEED_CURVE      30
// 以下宏供 Task 2/5 的 count 驱动弧段使用
#define RIGHT_HALF_CIRCLE  (-185.0f)
#define LEFT_HALF_CIRCLE   (+185.0f)
#define YAW_THRESHOLD         8.0f
// 对角线角度（供 Task 3/5 使用）
#define DIAG_AC_ANGLE   (-38.0f)
#define DIAG_BD_ANGLE   (-144.0f)

// ---- Task 3 进弯刹车 / 出弯缓冲参数 ----
#define SPEED_STRAIGHT       60    // 直道速度
#define SPEED_CURVE_RUN      30    // 弯道循迹速度
#define TICK_IN_BRAKE        10    // 进弯重刹持续 tick (10×20ms=200ms)
#define SPEED_IN_BRAKE       0     // 进弯刹车速度 (点刹减速)
#define TICK_OUT_BUFFER      8     // 出弯中速保护 tick (8×20ms=160ms)
// ---- YawTrack 弧段角度阈值 (P1#5: 按实际角度校准，取代一刀切 180°/200°) ----
// 场地参数: 圆弧半径 40cm，半圆弧理论偏航 ≈90°，小弧段 ≈36-50°，对角线 ≈38°/144°
#define YAW_THRESH_HALF_ARC   110.0f   // 半圆弧 (Task 2/5): ~90° + 20° 余量
#define YAW_THRESH_SMALL_ARC   60.0f   // 小弧段 (Task 3/4 C→B, D→A): ~36-50° + 余量
#define YAW_THRESH_DIAG       170.0f   // 对角线 (Task 3/4 AC/BD): 38°/144° + 余量
#define YAW_THRESH_SHORT_ARC   60.0f   // 短弧段 (Task 4 过渡): ~36° + 余量

// ---- 节点计数去抖 (H0 基础设施) ----
#define DEBOUNCE_INIT 10 // 去抖窗口 10 * 20ms = 200ms
volatile uint8_t count_debounce = 0;     // volatile: ISR 与主循环同时写入

// ---- 前馈差速 (H0 基础设施) ----
int16_t ff_diff = 0;

uint8_t lap_count = 0;
uint8_t rotating = 0;              // 1=正在原地旋转，屏蔽 K230
uint16_t selected_task = 1;
volatile uint16_t task_running = 0;     // volatile: ISR 写入（盲开超时停车），主循环读取
uint8_t current_state = 0;
uint8_t last_line_status = 0x00;
volatile uint16_t count = 0;             // volatile: ISR 与主循环双重递增；uint16_t 防 Task4 长时间溢出
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
            rotating = 0;
            angle_initialized = 0;   // P0#2: 角度过滤器哨兵归零，首帧无条件建立基线
            ControlState_Reset();    // P1+P2: 清零blind_ticks + turn_out_smooth + PID
            YawTrack_Reset(current_angle);
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
    static int t2_last_count = -1;

    // 检测 count 变化：首次进入新路段时调用 Reset
    if (count != t2_last_count)
    {
        t2_last_count = count;
        // 进入弯道段时重置角度累计追踪
        if (count == 1 || count == 3)
            YawTrack_Reset(current_angle);
    }

    // 弯道段兜底：半圆弧 ~90° → 阈值 110° (P1#5: 按实际角度校准)
    // 加去抖窗口，防止 ISR 中 count++ 与此处竞争导致跳过路段
    if ((count == 1 || count == 3) && YawTrack_IsCurveDone(YAW_THRESH_HALF_ARC)) {
        __disable_irq();   // P2#7: 保护 count++ + count_debounce 原子性
        count++;
        count_debounce = DEBOUNCE_INIT;
        __enable_irq();
    }

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
// Task 3: 赛题(3) A→C→B→D→A 1圈（count 驱动 + 进弯刹车/出弯缓冲）
//         导航策略参考 0603 Run_Task_3
// ================================================================
static void Run_Task_3(void)
{
    static int t3_wait_tick = 0;
    static int t3_last_count = -1;

    // 【核心机制】count 改变时重置计时器和角度追踪
    if (count != t3_last_count)
    {
        t3_wait_tick = 0;
        t3_last_count = count;
        // 进入弯道段时重置角度累计追踪
        if (count == 1 || count == 2 || count == 3)
            YawTrack_Reset(current_angle);
    }

    // 弯道段兜底：按各弧段实际角度校准阈值 (P1#5)
    // count==1 C→B ~38°→60°, count==2 B→D ~144°→170°, count==3 D→A ~36°→60°
    if (count == 1 || count == 2 || count == 3) {
        float yt = (count == 2) ? YAW_THRESH_DIAG : YAW_THRESH_SMALL_ARC;
        if (YawTrack_IsCurveDone(yt)) {
            __disable_irq();   // P2#7: 保护 count++ + count_debounce 原子性
            count++;
            count_debounce = DEBOUNCE_INIT;
            __enable_irq();
        }
    }

    switch (count)
    {
        case 0:
            // ---------------- A→C 对角线段 (起步先转头) ----------------
            t3_wait_tick++;
            if (t3_wait_tick <= 15) // 约 300ms 纯原地转头，对准 -38°
            {
                target_angle = DIAG_AC_ANGLE;
                base_speed = 0;
                rotating = 1;
            }
            else
            {
                target_angle = DIAG_AC_ANGLE;
                base_speed = SPEED_STRAIGHT;   // 60，盲开冲刺 A->C
                rotating = 0;
            }
            ff_diff = 0;
            break;

        case 1:
            // ---------------- C→B 段 (进弯刹车磨合期) ----------------
            t3_wait_tick++;
            if (t3_wait_tick <= TICK_IN_BRAKE)
            {
                target_angle = DIAG_AC_ANGLE;  // 刹车期间保持上一段角度，防原地打转
                base_speed = SPEED_IN_BRAKE;   // 进弯重刹
            }
            else
            {
                target_angle = 0.0f;           // 刹车结束后切入新弯道
                base_speed = SPEED_CURVE_RUN;  // 正常循迹过弯
            }
            ff_diff = 0;
            break;

        case 2:
            // ---------------- B→D 段 (进弯刹车) ----------------
            t3_wait_tick++;
            if (t3_wait_tick <= TICK_IN_BRAKE)
            {
                target_angle = 0.0f;           // 刹车期间保持上一段角度 (C->B是0度)
                base_speed = SPEED_IN_BRAKE;
            }
            else
            {
                target_angle = DIAG_BD_ANGLE;  // -144°
                base_speed = SPEED_CURVE_RUN;
            }
            ff_diff = 0;
            break;

        case 3:
            // ---------------- D→A 段 (进弯刹车) ----------------
            t3_wait_tick++;
            if (t3_wait_tick <= TICK_IN_BRAKE)
            {
                target_angle = DIAG_BD_ANGLE;  // 刹车期间保持上一段角度 (-144度)
                base_speed = SPEED_IN_BRAKE;
            }
            else
            {
                target_angle = 180.0f;         // D->A 是 180度
                base_speed = SPEED_CURVE_RUN;
            }
            ff_diff = 0;
            break;

        case 4:
        default:
            // 回到 A 点，停车
            task_running = 0;
            base_speed = 0;
            count = 0;
            t3_wait_tick = 0;
            t3_last_count = -1;
            ff_diff = 0;
            rotating = 0;
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
            YawTrack_Reset(current_angle);
            current_state = 1;
            break;

        // 状态 1: 第一圈 - 保持对角推进 A→C (~38°) -> 等待触发 C 点
        case 1:
            target_angle = -38.0f;
            base_speed = 60;
            ff_diff = 0;

            if (count >= 1 || YawTrack_IsCurveDone(YAW_THRESH_SHORT_ARC))
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
            YawTrack_Reset(current_angle);
            current_state = 3;
            break;

        // 状态 3: 第一圈 - 保持 0 度推进 C→B (~50°) -> 等待 B 点
        case 3:
            base_speed = 30;
            target_angle = 0.0f;
            ff_diff = 0;

            if (count >= 2 || YawTrack_IsCurveDone(YAW_THRESH_SMALL_ARC))
            {
                base_speed = 0;
                current_state = 4;
            }
            break;

        // 状态 4: 第一圈 - B点换方向 -> 目标 -146度 (逐圈微调)
        case 4:
            base_speed = 60;
            target_angle = -146.0f;
            count = 2;
            YawTrack_Reset(current_angle);
            current_state = 5;
            break;

        // 状态 5: 第一圈 - 保持对角返回 B→D (~144°) -> 等待 D 点
        case 5:
            base_speed = 60;
            target_angle = -146.0f;
            ff_diff = 0;

            if (count >= 3 || YawTrack_IsCurveDone(YAW_THRESH_DIAG))
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
            YawTrack_Reset(current_angle);
            current_state = 7;
            break;

        // 状态 7: 第一圈 - 直行冲向 A 点 (直道 ~0°) -> 触发后切入第二圈
        case 7:
            base_speed = 30;
            target_angle = 180.0f;
            ff_diff = 0;

            if (count >= 4 || YawTrack_IsCurveDone(YAW_THRESH_SHORT_ARC))   // 直道: count 为主

                base_speed = 0;
                count = 0;
                current_state = 10;
                wait_tick = 0;
            }
            break;


        // ========================================================
        // 【第二圈】 状态 10 ~ 17
        // ========================================================

        // 状态 10: 第二圈 - A点起步 -> 目标 -34度 (逐圈微调)
        case 10:
            base_speed = 60;
            target_angle = -34.0f;
            count = 0;
            YawTrack_Reset(current_angle);
            current_state = 11;
            break;

        // 状态 11: 第二圈 - 保持对角推进 A→C (~34°) -> 等待触发 C 点
        case 11:
            target_angle = -34.0f;
            base_speed = 60;

            if (count >= 1 || YawTrack_IsCurveDone(YAW_THRESH_SHORT_ARC))
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
            YawTrack_Reset(current_angle);
            current_state = 13;
            break;

        // 状态 13: 第二圈 - 保持 0 度推进 C→B (~50°) -> 等待 B 点
        case 13:
            base_speed = 30;
            target_angle = 0.0f;

            if (count >= 2 || YawTrack_IsCurveDone(YAW_THRESH_SMALL_ARC))
            {
                base_speed = 0;
                current_state = 14;
            }
            break;

        // 状态 14: 第二圈 - B点换方向 -> 目标 -148度 (逐圈微调)
        case 14:
            base_speed = 60;
            target_angle = -148.0f;
            count = 2;
            YawTrack_Reset(current_angle);
            current_state = 15;
            break;

        // 状态 15: 第二圈 - 保持对角返回 B→D (~148°) -> 等待 D 点
        case 15:
            base_speed = 60;
            target_angle = -148.0f;

            if (count >= 3 || YawTrack_IsCurveDone(YAW_THRESH_DIAG))
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
            YawTrack_Reset(current_angle);
            current_state = 17;
            break;

        // 状态 17: 第二圈 - 直行冲向 A 点 (直道 ~0°) -> 触发后切入最后一圈
        case 17:
            base_speed = 30;
            target_angle = 180.0f;

            if (count >= 4 || YawTrack_IsCurveDone(YAW_THRESH_SHORT_ARC))
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

        // 状态 20: 第三圈 - A点起步 -> 目标 -37度 (逐圈微调)
        case 20:
            base_speed = 60;
            target_angle = -37.0f;
            count = 0;
            YawTrack_Reset(current_angle);
            current_state = 21;
            break;

        // 状态 21: 第三圈 - 保持对角推进 A→C (~37°) -> 等待触发 C 点
        case 21:
            target_angle = -37.0f;
            base_speed = 60;

            if (count >= 1 || YawTrack_IsCurveDone(YAW_THRESH_SHORT_ARC))
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
            YawTrack_Reset(current_angle);
            current_state = 23;
            break;

        // 状态 23: 第三圈 - 保持 0 度推进 C→B (~50°) -> 等待 B 点
        case 23:
            base_speed = 30;
            target_angle = 0.0f;

            if (count >= 2 || YawTrack_IsCurveDone(YAW_THRESH_SMALL_ARC))
            {
                base_speed = 0;
                current_state = 24;
            }
            break;

        // 状态 24: 第三圈 - B点换方向 -> 目标 -146度 (逐圈微调)
        case 24:
            base_speed = 60;
            target_angle = -146.0f;
            count = 2;
            YawTrack_Reset(current_angle);
            current_state = 25;
            break;

        // 状态 25: 第三圈 - 保持对角返回 B→D (~147°) -> 等待 D 点
        //          (最后一圈 angle 微调为 -147°)
        case 25:
            base_speed = 60;
            target_angle = -147.0f;

            if (count >= 3 || YawTrack_IsCurveDone(YAW_THRESH_DIAG))
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
            YawTrack_Reset(current_angle);
            current_state = 27;
            break;

        // 状态 27: 第三圈 - 直行冲向终点 A 点 (直道 ~0°)
        case 27:
            base_speed = 30;
            target_angle = 180.0f;

            if (count >= 4 || YawTrack_IsCurveDone(YAW_THRESH_SHORT_ARC))
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
    static int t5_last_count = -1;

    // 检测 count 变化：首次进入新路段时调用 Reset
    if (count != t5_last_count)
    {
        t5_last_count = count;
        if (count == 1 || count == 3)
            YawTrack_Reset(current_angle);
    }

    if (count >= 4)
    {
        lap_count++;
        if (lap_count >= 4)
        {
            task_running = 0;
            base_speed = 0;
            ff_diff = 0;
            lap_count = 0;
            t5_last_count = -1;
            return;
        }
        else
        {
            count = 0;
        }
    }

    // 弯道段兜底：半圆弧 ~90° → 阈值 110° (P1#5: 按实际角度校准)
    // 加去抖窗口，防止 ISR 中 count++ 与此处竞争导致跳过路段
    if ((count == 1 || count == 3) && YawTrack_IsCurveDone(YAW_THRESH_HALF_ARC)) {
        __disable_irq();   // P2#7: 保护 count++ + count_debounce 原子性
        count++;
        count_debounce = DEBOUNCE_INIT;
        __enable_irq();
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
