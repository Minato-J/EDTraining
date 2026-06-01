#!/bin/bash
# 保护 ST 官方 HAL 库和 Keil 工程文件不被直接修改
# stdin 输入格式：{ "tool_name", "tool_input": { "file_path": ... }, ... }

INPUT=$(cat)

FILE_PATH=$(echo "$INPUT" | python -c 'import json,sys
try:
    d = json.loads(sys.stdin.read())
    fp = d.get("tool_input", {}).get("file_path", "")
    print(fp)
except Exception:
    pass' 2>/dev/null)

if [ -z "$FILE_PATH" ]; then
  exit 0
fi

# 统一用正斜杠匹配
FILE_PATH_NORM=$(echo "$FILE_PATH" | sed 's|\\|/|g')

if echo "$FILE_PATH_NORM" | grep -qE 'H0/Drivers/'; then
  echo "BLOCKED: ST 官方 HAL 库文件不允许直接修改，请通过 STM32CubeMX 重新生成" >&2
  exit 2
fi

if echo "$FILE_PATH_NORM" | grep -qE '\.uvproj'; then
  echo "BLOCKED: Keil 工程文件不允许直接修改，请在 Keil IDE 中操作" >&2
  exit 2
fi

exit 0
