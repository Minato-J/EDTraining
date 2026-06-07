# 06f — 清理 rotating + 最终收尾

**Status:** ready-for-agent

## Parent

Issue 06 — ctrl_mode 显式控制模式大重构

## What to build

移除已废弃的 `rotating` 变量，清理残留引用，编译验证。

### task.h 改动

删除：
```c
extern uint8_t rotating;         // 旋转阶段标志 (1=正在原地旋转, 屏蔽K230)
```

### task.c 改动

删除：
```c
uint8_t rotating = 0;              // 1=正在原地旋转，屏蔽 K230
```

### main.c 改动

删除（如果还存在）：
```c
extern uint8_t rotating;       // 旋转阶段标志 (task.c)
```

### 验证无残留引用

确保代码中不再出现 `rotating` 标识符（搜索确认）。

## Acceptance criteria

- [ ] 编译零错误零警告
- [ ] `rotating` 标识符在整个项目中无引用
- [ ] 所有 5 个 Task 行为不变

## Blocked by

- [06e-task-4-migration] — 所有 Task 迁移完成后才能清理
