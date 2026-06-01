#!/usr/bin/env bash
# embedded-safety-check.sh
# PostToolUse hook: 每次 Edit/Write 后自动检查嵌入式常见坑
#
# 用法: 由 .claude/settings.json hooks.PostToolUse 调用
# 环境变量: $TOOL_INPUT_FILE_PATH 由 Claude Code harness 注入

set -euo pipefail

FILE="${TOOL_INPUT_FILE_PATH:-}"

# 只检查 H0/ 下的 .c 和 .h 文件
if [[ -z "$FILE" ]] || [[ ! "$FILE" =~ \.(c|h)$ ]] || [[ ! "$FILE" =~ H0/ ]]; then
  exit 0
fi

WARNINGS=""

# ============================================================
# 1. 中断回调中是否调用了阻塞/不安全函数
# ============================================================
# 检查 HAL_TIM_PeriodElapsedCallback 或 HAL_UART_RxCpltCallback 中
# 是否有 printf / HAL_Delay / malloc / free
if grep -n 'HAL_TIM_PeriodElapsedCallback\|HAL_UART_RxCpltCallback' "$FILE" >/dev/null 2>&1; then
  IN_ISR=$(awk '
    /HAL_TIM_PeriodElapsedCallback|HAL_UART_RxCpltCallback/ { found=1 }
    found && /printf\(/ { print NR": printf() in ISR callback (may cause HardFault or blocking)" }
    found && /HAL_Delay\(/ { print NR": HAL_Delay() in ISR callback (will deadlock)" }
    found && /malloc\(|free\(/ { print NR": malloc/free in ISR callback (not reentrant)" }
  ' "$FILE" 2>/dev/null)
  if [[ -n "$IN_ISR" ]]; then
    WARNINGS+="🔴 ISR 不安全调用:\n$IN_ISR\n\n"
  fi
fi

# ============================================================
# 2. 共享变量是否标记 volatile
# ============================================================
ISR_SHARED_VARS="target_angle base_speed ff_diff count task_running current_state line_sensor_data last_line_status"

for VAR in $ISR_SHARED_VARS; do
  if grep -qE "^(extern\s+|static\s+)?(uint[0-9]*_t|int[0-9]*_t|float|int)\s+${VAR}\b" "$FILE" 2>/dev/null; then
    if ! grep -qE "volatile.*\b${VAR}\b|\b${VAR}\b.*volatile" "$FILE" 2>/dev/null; then
      WARNINGS+="🟡 变量 '${VAR}' 在 ISR 与主循环共享，建议加 volatile 或临界区保护\n"
    fi
  fi
done

# ============================================================
# 3. 编码器读取是否有低通滤波
# ============================================================
if grep -qE 'Encoder_Get_Count' "$FILE" 2>/dev/null; then
  if ! grep -qE 'fil_l|fil_r|smooth|filter' "$FILE" 2>/dev/null; then
    WARNINGS+="🟡 编码器原始值直接使用，建议添加低通滤波减少噪声\n"
  fi
fi

# ============================================================
# 4. PID 输出限幅检查
# ============================================================
if grep -qE 'PID_Init\(' "$FILE" 2>/dev/null; then
  ZERO_MAX=$(grep -nE 'PID_Init\([^)]+,\s*0(\.0f?)?\s*\)' "$FILE" 2>/dev/null)
  if [[ -n "$ZERO_MAX" ]]; then
    WARNINGS+="🔴 PID_Init max=0 可能导致输出被截断:\n$ZERO_MAX\n\n"
  fi
fi

# ============================================================
# 5. while(wait_for_release) 阻塞检查
# ============================================================
BLOCKING_WAIT=$(grep -nE 'while\s*\(\s*HAL_GPIO_ReadPin' "$FILE" 2>/dev/null)
if [[ -n "$BLOCKING_WAIT" ]]; then
  WARNINGS+="🟡 阻塞式按键等待（可能饿死主循环）:\n$BLOCKING_WAIT\n\n"
fi

# ============================================================
# 输出结果
# ============================================================
if [[ -n "$WARNINGS" ]]; then
  echo "⚡ 嵌入式安全检查 ($FILE):"
  echo -e "$WARNINGS"
fi

exit 0
