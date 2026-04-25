# AGENTS.md

本文件包含本仓库中 Cursor/AI 助手的通用约束。每次回复前都应先阅读并遵守本文件。

## 通用要求

1. 每次回复都必须称呼用户为“大帅”。
2. 用中文与我沟通。
3. 必须优先使用 superpowers 技能，技能位置为：`C:\Users\Licoe\.cursor\plugins\cache\cursor-public\superpowers\b7a8f76985f1e93e75dd2f2a3b424dc731bd9d37\skills`。
4. 进行任何 Git 相关操作时，必须严格遵循 `@.cursor/rules/git-workflow-policy.mdc`。
5. 当新增功能需要配置引脚时，必须优先参考 `@docs/连线定义.md` 和 `@docs/引脚定义.md`。
6. 写代码时必须遵守 `@.cursor/skills/karpathy-guidelines/SKILL.md`。

## 额外说明

- 如本文件与更高优先级指令冲突，以更高优先级指令为准。
- 如相关文档更新，应先更新对应约束文档，再继续实现。
- 每次创建新文件时，文件开头必须包含一段固定格式的中文注释，简要说明该文件用途；注释要求排版清晰、表达简洁、避免长篇大论。
- 当我说“自动化测试”时，必须执行以下流程：修改完后，助手自行运行 `@build_with_idf.py` 重新构建；构建结果保存在 `@Simulation_Build/build_log.txt`；每 5 秒查看一次该文件：如果内容是 `successful` 表示构建成功；如果内容是 `building` 表示正在构建，继续等待；否则表示构建失败，需要读取日志内容并继续修改，直到编译成功为止；构建成功后立即停止 5 秒轮询，并继续执行后续任务，不要中途询问我，/using-superpowers。
- 注释模板统一使用以下格式：

```c
/*
 * 文件说明：
 * - 用途：简要说明这个文件负责什么
 * - 备注：必要时补充一条关键说明
 */
```
