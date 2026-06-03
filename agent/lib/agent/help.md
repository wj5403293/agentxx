# DeepAgent 类文档

## 概述

`DeepAgent` 是 agentxx 框架中的核心智能体类，用于实现基于图执行的代理系统。它结合了 LLM（大语言模型）、工具调用和中间件处理能力，能够执行复杂的任务链。

## 主要组件

### 1. 依赖项与头文件

```cpp
#include "agent/config.h"
#include "asio/co_spawn.hpp"
#include "asio/detached.hpp"
#include "asio/io_context.hpp"
#include "middlewares/skill.h"
#include "neograph/llm/openai_provider.h"
#include "neograph/mcp/client.h"
#include "neograph/neograph.h"
#include "nodes/agentcall.h"
#include "nodes/modelcall.h"
#include "nodes/toolcall.h"
#include "tools/execute_command.h"
#include "tools/filesystem.h"
#include "tools/get_current_datetime.h"
#include "tools/skill.h"
#include "tools/string.h"
#include "tools/websearch.h"
```

### 2. 核心成员变量

#### 调度器与引擎
- `ioCtx`: ASIO I/O 上下文，用于异步协程调度
- `engine`: NeoGraph 图引擎实例，执行节点流程
- `config`: Agentxx 配置对象
- `middlewareHandleContext`: 中间件处理上下文字

### 3. 预置工具列表 (Toolcalls)

#### 时间相关
- `GetCurrentDateTimeTool` - 获取当前日期和时间

#### 网络搜索
- `WebSearchTool` - Web 搜索
- `FetchUrlTool` - 获取 URL 内容
- `FetchUrlMarkdownTool` - 以 Markdown 格式获取 URL 内容

#### 文件系统操作
- `FileSystemListFileTool` - 列出文件/文件夹信息
- `FilesystemReadTextFileTool` - 读取文本文件
- `FilesystemReadBinaryFileTool` - 读取二进制文件
- `FilesystemWriteFileTool` - 创建或覆盖文件
- `FilesystemEditTextFileTool` - 编辑文本文件（字符串替换）
- `FilesystemGlobTool` - 匹配模式查找文件

#### 字符串处理
- `StringHtml2MarkdownTool` - HTML 转 Markdown
- `StringRegexpTool` - 正则表达式搜索/替换/删除

#### 命令执行 (平台相关)
- Linux: `ExecuteLinuxCommandTool`
- Windows: `ExecuteWindowsCommandTool`
- WSL 环境下同时支持两者

### 4. 初始化流程 (`init()`)

#### 步骤 1: 工具注册
按以下顺序添加工具：
1. 基础系统工具（时间、搜索、文件操作等）
2. MCP (Model Context Protocol) 服务器工具
3. 中间件扩展的工具

#### 步骤 2: 中间件配置
- **SkillMiddlewareHandle**: 技能中间件处理，指向 `/home/coolight/program/agentxx/isolation/skills/`
- **ToolcallLog Middleware**: 工具调用日志记录（开始和结束）

#### 步骤 3: 图节点注册
注册以下节点类型到 NodeFactory：
- `MiddlewareWrapAgentStartCallNode` - Agent 开始节点
- `MiddlewareWrapAgentEndCallNode` - Agent 结束节点
- `MiddlewareWrapModelCallNode` - 模型调用节点 (LLM)
- `MiddlewareWrapToolcallNode` - 工具调用节点

#### 步骤 4: 图引擎编译
构建执行图，流程如下：
```
__start__ → agent_start → llm 
                              ↓ (has_tool_calls?)
                             tools → llm (循环)
                              ↓ (no tool calls)
                            agent_end → __end__
```

### 6. 关键实现细节

#### ASIO I/O 上下文注意事项
```cpp
/// 警告：不要在同线程中使用 ioCtx 传递给 runCliAsync()
/// 原因: engine->run_stream_async 会启动其他 io_context
///       交替使用会导致互相等待，卡住程序
/// 解决方案: 通过 asio::this_coro::executor 获取当前绑定的 io_context
```

#### 配置要求
- `modelOpenAIBaseUrl` 必须非空
- `agentName` 需用于 MCP 客户端初始化

#### 图定义结构
```json
{
  "name": "<agent_name>",
  "channels": {"messages": {"type": "list", "reducer": "append"}},
  "nodes": ["agent_start", "agent_end", "llm", "tools"],
  "edges": [条件路由逻辑]
}
```

### 7. 内存管理

- **构造函数**: 创建 `ioCtx`，验证配置
- **析构函数**: 清空 `engine` 指针
- **所有权**: `engine->own_tools(std::move(tools))` 转移工具所有权

### 8. 使用示例

```cpp
// 创建配置并初始化 DeepAgent
auto config = createConfig(/* ... */);
DeepAgent agent(config);

// 初始化引擎
agent.init();

// 启动命令行交互
agent.runCli();
```

## 架构特点

1. **异步优先**: 基于 ASIO 协程的异步设计
2. **模块化工具**: 通过中间件动态扩展能力
3. **条件路由**: 智能判断是否需要调用工具
4. **流式输出**: 实时显示 LLM 生成内容
5. **平台自适应**: 自动适配 Linux/Windows/WSL

## 注意事项

1. **线程安全**: `ioCtx` 不能在多个 io_context 间传递
2. **资源清理**: 析构时会自动释放引擎
3. **配置验证**: 启动前会检查必要配置项
4. **异常处理**: 使用 try-catch 捕获运行时异常

## 相关模块

- [Config](../config.h) - 系统配置类
- [Middleware](../../middlewares/skill.h) - 中间件框架
- [NeoGraph](../../neograph/) - 图执行引擎
- [Tools](../../tools/) - 工具实现集合

---

*文档生成时间: $(date +%Y-%m-%d %H:%M:%S)*# DeepAgent 类文档
自动释放引擎
3. **配置验证**: 启动前会检查必要配置项
4. **异常处理**: 使用 try-catch 捕获运行时异常

## 相关模块

- [Config](../config.h) - 系统配置类
- [Middleware](../../middlewares/skill.h) - 中间件框架
- [NeoGraph](../../neograph/) - 图执行引擎
- [Tools](../../tools/) - 工具实现集合

---

*文档生成时间: $(date +%Y-%m-%d %H:%M:%S)*# DeepAgent 类文档
