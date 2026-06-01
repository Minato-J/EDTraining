---
name: issue-stub
description: 在 .scratch/ 下创建标准格式的问题 stub 文件
disable-model-invocation: true
---

用法：`/issue-stub <功能名> <简述>`

在 `.scratch/$1/` 下新建 `issue-<YYYYMMDD-HHmm>.md`，内容模板：

```markdown
---
title: $2
status: needs-triage
created: <当前时间>
---

## 现象

## 复现步骤

## 期望

## 关联文件
```

### 执行步骤

1. 解析参数：`$ARGUMENTS` 格式为 `<功能名> <简述>`，空格分隔
2. 获取当前时间
3. 创建目录 `.scratch/<功能名>/`（如已存在则跳过）
4. 用 Write 工具写入模板文件
5. 输出创建的文件路径
