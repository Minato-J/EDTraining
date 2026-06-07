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
 * Task 4 → 赛题(4) A→C→B→D→A 四圈，不停车（发挥）
 *          来源: Issue 10 重构 — 单圈模板 × LAP_TARGET(4) 循环 + 逐圈补偿表
 *          逐圈角度微调通过 lap_ac_comp[] / lap_bd_comp[] 查表，默认全零（不微调）
 * Task 5 → 扩展: A→C→B→D→A ×4圈（保留，不入赛题）
 *          来源: H0 原 Run_Task_4（count 驱动4圈），保留供调试
 *
 * H0 基础设施: count_debounce 节点去抖 + ff_diff 前馈差速
 *
 * 导航策略:
 *   Task 1/2: count 驱动连续循迹（K230 + 角度保持 + ff_diff）
 *   Task 3:   航点到航点，每节点停车→IMU旋转→冲刺
 *   Task 4:   航点到航点不停车，lap_count 驱动循环圈数 (#LAP_TARGET=4)
 *   Task 5:   count 驱动4圈连续循迹（保留）
 */

#include "task.h"
#include "main.h"
#include "stdio.h"
#include "wit_imu.h"
#include "k230_track.h"
#include "math.h"
#include "yaw_track.h"
#include "reach_point.h"   // Issue 03
#include "car_control.h"  // Issue 06: 显式控制模式（提供 target_angle/base_speed/ff_diff 声明）

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

// ---- 前馈差速 (H0 基础设施, 已移至 car_control.c Issue 06) ----

uint8_t lap_count = 0;
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
            angle_initialized = 0;   // P0#2: 角度过滤器哨兵归零，首帧无条件建立基线
            ControlState_Reset();    // P1+P2: 清零blind_ticks + turn_out_smooth + PID
            YawTrack_Reset(current_angle);
            K230_Timeout_Reset();   // Issue 02: Start 时清零 K230 超时
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
    static int t1_last_count = -1;

    // R5: 仅在 count 变化时调用 Car_StartLine，避免每 tick 重置 turn_out_smooth
    if (count != t1_last_count)
    {
        t1_last_count = count;
        if (count == 0)
        {
            Car_StartLine(0.0f, BASE_SPEED_STRAIGHT, 0);
        }
        else if (count == 1)
        {
            Car_Stop();
            task_running = 0;
            t1_last_count = -1;
        }
    }
}

// ================================================================
// Task 2: 赛题(2) A→B→C→D→A 外圈（count 驱动 + ff_diff）
// ================================================================
static void Run_Task_2(void)
{
    static int t2_last_count = -1;

    // R5: 仅在 count 变化时调用 Car_StartLine（模式切换 + 参数设置一次性完成）
    if (count != t2_last_count)
    {
        t2_last_count = count;
        // 进入弯道段时重置角度累计追踪
        if (count == 1 || count == 3)
            YawTrack_Reset(current_angle);

        switch (count)
        {
            case 0:
                // A→B 直线段，保持初始朝向
                Car_StartLine(0.0f, BASE_SPEED_STRAIGHT, 0);
                break;
            case 1:
                // B→C 右半圆弧
                Car_StartLine(RIGHT_HALF_CIRCLE, BASE_SPEED_CURVE, 10);
                break;
            case 2:
                // C→D 直线段，朝向 180°
                Car_StartLine(180.0f, BASE_SPEED_STRAIGHT, 0);
                break;
            case 3:
                // D→A 左半圆弧
                Car_StartLine(LEFT_HALF_CIRCLE, BASE_SPEED_CURVE, -10);
                break;
            case 4:
            default:
                // 回到 A，停车
                Car_Stop();
                task_running = 0;
                t2_last_count = -1;
                break;
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
}

// ================================================================
// Task 3: 赛题(3) A→C→B→D→A 1圈（count 驱动 + 进弯刹车/出弯缓冲）
//         导航策略参考 0603 Run_Task_3
// ================================================================
static void Run_Task_3(void)
{
    static int t3_wait_tick = 0;
    static int t3_last_count = -1;

    // R5: count 变化时 Car_StartLine 一次性完成模式切换 + 参数初始化
    if (count != t3_last_count)
    {
        t3_wait_tick = 0;
        t3_last_count = count;
        // 进入弯道段时重置角度累计追踪
        if (count == 1 || count == 2 || count == 3)
            YawTrack_Reset(current_angle);

        switch (count)
        {
            case 0:
                // A→C 对角线段
                Car_StartLine(DIAG_AC_ANGLE, SPEED_STRAIGHT, 0);
                break;
            case 1:
                // C→B 段：初始刹车速度，后续 tick 逐步放开
                Car_StartLine(0.0f, SPEED_IN_BRAKE, 0);
                break;
            case 2:
                // B→D 段：初始刹车速度
                Car_StartLine(DIAG_BD_ANGLE, SPEED_IN_BRAKE, 0);
                break;
            case 3:
                // D→A 段：初始刹车速度
                Car_StartLine(180.0f, SPEED_IN_BRAKE, 0);
                break;
            case 4:
            default:
                // 回到 A 点，停车
                Car_Stop();
                task_running = 0;
                count = 0;
                t3_wait_tick = 0;
                t3_last_count = -1;
                return;  // Car_Stop 已处理，跳过后续刹车调整
        }
    }
    else
    {
        // R5: 同一路段后续 tick — 仅调整动态刹车速度，不重置模式和 turn_out_smooth
        if (count == 1 || count == 2 || count == 3)
        {
            t3_wait_tick++;
            Car_SetSpeed((t3_wait_tick <= TICK_IN_BRAKE) ? SPEED_IN_BRAKE : SPEED_CURVE_RUN);
        }
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
}

// ================================================================
// Task 4: 赛题(4) A→C→B→D→A 四圈不停车（Issue 10 重构: 单圈模板 × LAP_TARGET 循环）
//         逐圈角度补偿通过查表，默认全零（实测标定后再填入）
// ================================================================
#define LAP_TARGET 4  // 赛题(4): 4 圈

// 逐圈角度补偿表（索引 0~3 对应第 1~4 圈，单位: 度）
// 默认全零 = 不微调；实测若需补偿，按圈填入偏移量即可
static const float lap_ac_comp[LAP_TARGET] = {0.0f, 0.0f, 0.0f, 0.0f};
static const float lap_bd_comp[LAP_TARGET] = {0.0f, 0.0f, 0.0f, 0.0f};

// 基准角度（不随圈数变化）
#define AC_BASE_ANGLE  (-38.0f)
#define BD_BASE_ANGLE  (-146.0f)

static void Run_Task_4(void)
{
    uint8_t step = current_state;               // 0~7 圈内步骤
    uint8_t lap  = lap_count;                   // 0-based: 0=第1圈 … 3=第4圈

    // 本圈补偿后的目标角度
    float ac_angle = AC_BASE_ANGLE + lap_ac_comp[lap];
    float bd_angle = BD_BASE_ANGLE + lap_bd_comp[lap];

    switch (step)
    {
        // ========================================================
        // 状态 0: A点起步 → 设对角线 AC 角度
        // ========================================================
        case 0:
            Car_StartLine(ac_angle, 60, 0);
            count = 0;
            YawTrack_Reset(current_angle);
            current_state = 1;
            break;

        // ========================================================
        // 状态 1: A→C 对角推进 → 等 C 点触发
        // ========================================================
        case 1:
            Car_SetTargetAngle(ac_angle);
            Car_SetSpeed(60);
            Car_SetFFDiff(0);

            if (count >= 1 || YawTrack_IsCurveDone(YAW_THRESH_SHORT_ARC))
            {
                Car_SetSpeed(0);
                current_state = 2;
            }
            break;

        // ========================================================
        // 状态 2: C点换向 → 目标 0°（弧段 C→B）
        // ========================================================
        case 2:
            Car_StartLine(0.0f, 30, 0);
            count = 1;
            YawTrack_Reset(current_angle);
            current_state = 3;
            break;

        // ========================================================
        // 状态 3: C→B 弧段推进 → 等 B 点触发
        // ========================================================
        case 3:
            Car_SetSpeed(30);
            Car_SetTargetAngle(0.0f);
            Car_SetFFDiff(0);

            if (count >= 2 || YawTrack_IsCurveDone(YAW_THRESH_SMALL_ARC))
            {
                Car_SetSpeed(0);
                current_state = 4;
            }
            break;

        // ========================================================
        // 状态 4: B点换向 → 设对角线 BD 角度
        // ========================================================
        case 4:
            Car_StartLine(bd_angle, 60, 0);
            count = 2;
            YawTrack_Reset(current_angle);
            current_state = 5;
            break;

        // ========================================================
        // 状态 5: B→D 对角推进 → 等 D 点触发
        // ========================================================
        case 5:
            Car_SetSpeed(60);
            Car_SetTargetAngle(bd_angle);
            Car_SetFFDiff(0);

            if (count >= 3 || YawTrack_IsCurveDone(YAW_THRESH_DIAG))
            {
                Car_SetSpeed(0);
                current_state = 6;
            }
            break;

        // ========================================================
        // 状态 6: D点换向 → 目标 180°（弧段 D→A）
        // ========================================================
        case 6:
            Car_StartLine(180.0f, 30, 0);
            count = 3;
            YawTrack_Reset(current_angle);
            current_state = 7;
            break;

        // ========================================================
        // 状态 7: D→A 弧段推进 → 到 A 点判断圈数
        // ========================================================
        case 7:
            Car_SetSpeed(30);
            Car_SetTargetAngle(180.0f);
            Car_SetFFDiff(0);

            if (count >= 4 || YawTrack_IsCurveDone(YAW_THRESH_SHORT_ARC))
            {
                Car_SetSpeed(0);
                lap_count++;
                if (lap_count >= LAP_TARGET)
                {
                    // 全部圈数完成 → 停车
                    Car_Stop();
                    task_running = 0;
                    current_state = 0;
                    count = 0;
                    wait_tick = 0;
                    lap_count = 0;
                }
                else
                {
                    // 还有下一圈 → 回到状态 0
                    count = 0;
                    current_state = 0;
                    wait_tick = 0;
                }
            }
            break;

        // ========================================================
        // default: 安全停车
        // ========================================================
        default:
            Car_Stop();
            task_running = 0;
            current_state = 0;
            count = 0;
            wait_tick = 0;
            lap_count = 0;
            break;
    }
}

// ================================================================
// Task 5: 扩展 A→C→B→D→A ×4圈（来源: H0 原 Run_Task_4，保留）
// ================================================================
static void Run_Task_5(void)
{
    static int t5_last_count = -1;

    // R5: count 变化时 Car_StartLine 一次性完成模式切换 + 参数设置
    if (count != t5_last_count)
    {
        t5_last_count = count;
        // 进入弯道段时重置角度累计追踪
        if (count == 1 || count == 3)
            YawTrack_Reset(current_angle);

        switch (count)
        {
            case 0:
                Car_StartLine(DIAG_AC_ANGLE, BASE_SPEED_STRAIGHT, 0);
                break;
            case 1:
                Car_StartLine(RIGHT_HALF_CIRCLE, BASE_SPEED_CURVE, 10);
                break;
            case 2:
                Car_StartLine(DIAG_BD_ANGLE, BASE_SPEED_STRAIGHT, 0);
                break;
            case 3:
                Car_StartLine(LEFT_HALF_CIRCLE, BASE_SPEED_CURVE, -10);
                break;
        }
    }

    if (count >= 4)
    {
        lap_count++;
        if (lap_count >= 4)
        {
            Car_Stop();
            task_running = 0;
            lap_count = 0;
            t5_last_count = -1;
            return;
        }
        else
        {
            // R5: 圈数过渡 — 立即设 count=0 参数，避免等待下一 tick
            count = 0;
            t5_last_count = 0;
            Car_StartLine(DIAG_AC_ANGLE, BASE_SPEED_STRAIGHT, 0);
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
        Car_Stop();   // Issue 06: 停工时切 CTRL_PARK，替代 base_speed=0
    }
}
