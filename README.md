## 简介
[Github agentxx](https://github.com/coolight7/agentxx)

- c++实现 AI Agent，为了减少内存占用、程序包体积，用于普通用户的手机电脑运行而设计

## 计划实现
### 基础模块
- Toolcall:
    - ✅返回值自动转换字符编码到 utf8
    - ✅filesystem（已支持 同步 + asio协程异步 文件读写）
        - ls
        - read_text (full / offset + limit)
        - read_binary (full / byte offset + limit)
        - write (text/binary)
        - edit_text
        - glob
        - grep (text/regex)
    - ✅exec_shell
        - execute_linux_command
        - execute_windows_command (检测到 WSL 环境时，允许在 linux/wsl 直接执行 windows 命令)
        - ⬜execute_python_command
        - ⬜execute_javascript_command
    - ✅web_search
        - search
        - fetch_url_md (html to markdown)
        - fetch_url (raw resp body)
    - ✅tree-messages
        - temp_kvstore（会话独立，提供 K-V Store允许模型存取变量）
        - 消息分支，支持修改历史消息/模型重新生成消息
    - ✅sub-agent
        - subagent_task (隔离上下文)
        - ⬜skill_search + tool_search (延迟加载 mcp tool)
        - ⬜subagent_audio_generate
        - ⬜subagent_image_generate
        - ⬜subagent_video_generate
    - ✅todo_list
    - ⬜self-upgrade
        - 自动循环调整系统提示词、工具提示词等，评估效果
    - ⬜ui_control
        - 接收音视频/图片/文本输入，输出键鼠控制
- ✅Middleware支持
    - 支持层次化栈式拦截 (层层执行 start，压栈对应的 end，再逐层向外退栈执行 end) `agentCallStart`、`agentCallEnd`、`modelCallStart`、`modelCallEnd`、`toolCallStart`、`toolCallEnd`
- ✅压缩上下文`SummarizationMiddleware`
    - 自动估算 tokens，达到阈值时自动压缩
    - 裁剪历史消息中过时的文件读写消息
    - 将部分重要的长消息内容暂存到 `temp_kvstore`，而不压缩，模型需要时可以提取
    - LLM 总结压缩
    - 保留最近消息
- ✅Skill支持`SkillMiddlewareHandle`
    - 文件夹扫描/metadata读取收集 + `filesystem`文件内容读取 + `exec_shell`执行
    - ⬜动态加载 load/offload 
- ❌MCP支持（Neograph已实现，但暂时使用有问题）
- ⬜扩展
    - CodeGraph
    - RAG
    - 语音识别
    - 文本转语音
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
- ⬜操作live2d模型动作

## 目录结构
- `agent`:
    - c++实现agent
- `skill`:
    - 将加载的 skill 插入到上下文开头
    - 将加载的 skill 插入到上下文末尾
    - 保留加载的skill在上下文 toolcall 位置不动，压缩上下文时整理 skill 到开头（需要确认效果对比整合在一起）
- third_party
    - `neo-graph`: 图执行核心
    - `codegraph-cpp`: 分析代码/md文件关系

## 编译 
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
