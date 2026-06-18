# Agentxx
[Github agentxx](https://github.com/coolight7/agentxx)

- C++ 实现 AI Agent，减少内存占用、程序包体积，为普通性能的手机、电脑等设备上运行而设计

## 计划实现
### 基础模块
- Toolcall:
    - ✅返回值自动转换字符编码到 utf8
    - ✅filesystem (支持 `同步`/`asio 协程异步` 文件读写)
        - ls (file/dir/recursive-dir/limit)
        - read_text (full / offset-limit)
        - read_binary (full / byte-offset-limit)
        - write (text/binary)
        - edit_text
        - glob
        - grep (multi text/regex multi-filepath)
    - ✅execute_command (支持 `同步`/`Boost.process 协程异步`执行)
        - execute_linux_command
        - execute_windows_command (检测到 WSL 环境时，允许在 linux/wsl 直接执行 windows 命令)
        - ⬜execute_python_command
        - ⬜execute_javascript_command
        - ⬜拦截输出，自动压缩、提取摘要存储到 share_store
    - ✅web_search
        - web_search (内置 HTML 转 markdown, 支持直接使用普通网页搜索api)
        - ⬜用 subagent 对接外部 llm agent 实现搜索
        - web_fetch_url_markdown (html to markdown)
        - web_fetch_url (raw resp body)
    - ✅planning
        - 分为两层规划
        - mermaid/stateDiagram-v2 状态图描述大方向的任务规划
        - todo_list 描述近期需要实现的任务细节步骤
    - ✅sub-agent
        - 一轮 Toolcall 支持并发启动多个 Subagent
        - subagent_task (隔离上下文)
    - ⬜tool_skill_search (延迟加载 tool/skill)
    - ⬜self-upgrade
        - 自动循环调整系统提示词、工具提示词等，评估效果
        - 自动测试
    - ✅get_current_datetime 获取系统时间戳、本地时间、UTC时间
- ✅Tree-Messages
    - share_store (会话独立，提供 Store 允许模型存取变量，skill、tool之间传递数据)
        - 支持 `line_offset`/`line_limit` 文本分页读取
        - 自动拦截 tool/subagent 返回值，太长时存储原始内容到 `share_store`, 并留下摘要和 id
    - ⬜消息摘要，支持存储原始消息到 `share_store` 后，能识别出 message content 是消息摘要
    - 消息分支，支持修改历史消息/模型重新生成消息
    - 多会话和历史会话
- ⬜Memory
    - 持久记忆
    - 总结共享记忆
    - 自定义加载记忆消息
- ⬜Permission
- ⬜异常处理和中断恢复
    - 用户终止执行
    - 节点超时处理
    - 异常中断恢复执行和自动重试
    - 主动暂停等待用户响应
- ✅Middleware支持
    - 支持层次化栈式拦截 (层层执行 start，压栈对应的 end，再逐层向外退栈执行 end) `agentCallStart`、`agentCallEnd`、`modelCallStart`、`modelCallEnd`、`toolCallStart`、`toolCallEnd`
- ✅压缩上下文`SummarizationMiddleware`
    - 自动估算 tokens，达到阈值时自动启动压缩
    - toolcall 各自实现压缩处理
        - 裁剪历史消息中过时的 (filesystem)文件读写、(planning)任务规划、(share_store)变量读写消息
    - 将部分重要的长消息内容暂存到 `share_store`，而不压缩，模型需要时可以提取
    - LLM 总结压缩
    - 保留最近消息
- ✅Skill支持`SkillMiddlewareHandle`
    - 文件夹扫描/metadata读取收集 + `filesystem`文件内容读取 + `execute_command`执行
- ❌MCP支持 (Neograph已实现，但暂时使用有问题)
    - MCP client
    - Mcp Server
        - CodeGraph
        - Websearch
- ⬜扩展
    - ✅CodeGraph
    - RAG
    - PaddleOCR (图片转文本)
    - SD.cpp 图片视频生成
    - FunASR 语音识别
    - Qwen3-TTS 文本转语音
- ⬜Server:
    - Openai api server
    - ACP server
    - A2A server

### 功能
- ⬜翻译/划词翻译
- ⬜根据文本/音视频，生成评论/弹幕
- ⬜根据图片内容，提取文本和提示并指定文本在图片上的位置
    - 实现类似游戏中图片内容中的多个提示点，点击扩展到文本内容或提示信息
- ⬜图片/视频生成，通过 llm 优化提示词后生成返回，可自动检查生成结果，调整提示词重新生成
- ⬜ASR/TTS
- ⬜匹配歌词
- ⬜操作键鼠
- ⬜操作live2d/3d模型动作

## 目录结构
- `agent`:
    - c++ 实现 Agent
- `agent/lib`:
    - 核心库，包含了内置实现的 toolcall、node、middleware 等，分离编译以便嵌入其他 app 开发使用
- `agent/client`:
    - 命令行可执行程序，计划用于启动服务、实现命令行用户交互
- `agent/test`:
    - 测试
- `agent/third_party`
    - `neo-graph`: 图执行核心
        - [原项目](https://github.com/fox1245/NeoGraph)
        - [Fork 修改](https://github.com/coolight7/NeoGraph)
            - Toolcall
                - 调整 Tool 默认为异步执行
                - 支持并发启动多个 Tool 同时开始执行
                - toolcall 增加参数 thread_id
                - McpTool 增加异步操作
            - NodeInput 允许修改 state，以便支持修改 messages 消息上下文
            - ChatMessage 支持记录修改历史，以实现记录模型重新生成消息、修改用户消息
            - LLMCallNode 当 messages 中存在 system message 时不再额外添加
            - GraphState 增加 overwrite 函数以支持强制覆盖变量
    - `codegraph-cpp`: 分析代码/md文件关系. 
        - [原项目](https://github.com/plutoaac/codegraph-cpp)
        - [Fork 修改](https://github.com/coolight7/codegraph-cpp):
            - 从支持 c++/python 解析，扩展到支持 js/ts/dart/rust/go/java/kotlin/bash/markdown 等 20+ 种编程语言和文件格式结构

## 编译 
- C++ Standard: Requires C++23 or higher.

- 编译器推荐
    - Linux/gcc 16.1. 此前使用 gcc 13.2 编译时，部分协程函数会导致编译器自身崩溃

- 拉取项目源码和依赖库
```sh
git clone https://github.com/coolight7/agentxx
cd agentxx
git submodule update --init
```

- 编译 Boost
```sh
# https://github.com/boostorg/boost
# 下载 release/boost-xxx.7z 解压到 agent/third_party/boost/
cd boost/
# 然后编译结果到 agent/third_party/boost-build/
./bootstrap.sh
./b2 --prefix=${PWD}/../boost-build
./b2 install --prefix=${PWD}/../boost-build
```

- 安装 codegraph-cpp 依赖
```sh
cd {项目根目录}/agent/third_party/codegraph-cpp
npm install --legacy-peer-deps
```

- 启动编译 agentxx，会自动下载其他依赖库，编译成功后自动运行 命令行 client
```sh
cd {项目根目录}/agent
./script/client_run.sh
```
