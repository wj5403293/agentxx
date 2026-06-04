## 简介
- c++实现 AI Agent，为了减少内存占用、程序包体积，用于普通用户的手机电脑运行而设计
- 依赖`NeoGraph`，仿照Python著名的 agent 框架`langgraph`，实现常用功能

## 计划实现
### 基础模块
- ⬜Toolcall:
    - ✅返回值自动转换字符编码到 utf8
    - ✅filesystem（已支持 同步 + asio协程异步 文件读写）
        - ls
        - read_text (full / offset+limit)
        - read_binary
        - write (text/binary)
        - edit_text
        - glob
        - ⬜grep
    - ✅exec_shell
        - execute_linux_command
        - execute_windows_command (检测到 WSL 环境时，允许在 linux/wsl 直接执行 windows 命令)
        - ⬜execute_python_command
        - ⬜execute_javascript_command
    - ✅web_search
        - search
        - fetch_url_md (html to markdown)
        - fetch_url (raw resp body)
    - ⬜tool_search (延迟加载 mcp tool)
    - ⬜temp_store（会话独立，作为类似内存，提供 K-V 允许模型存取变量）
    - ⬜ui_control
        - 接收音视频/图片/文本输入，输出键鼠控制
    - ⬜todo_list
    - ⬜tree-messages
        - 压缩上下文
        - sub-agent
        - 修改历史消息
        - 模型重新生成消息
- ✅Middleware支持
    - 支持层次化栈式拦截 (层层执行 start，压栈对应的 end，再逐层向外退栈执行 end) `agentCallStart`、`agentCallEnd`、`modelCallStart`、`modelCallEnd`、`toolCallStart`、`toolCallEnd`
- ✅Skill支持
    - 文件夹扫描/metadata读取收集 + `filesystem`文件内容读取 + `exec_shell`执行
    - ⬜实现 load/offload 动态加载
- ❌MCP支持（Neograph已实现，但暂时使用有问题）
- 扩展
    - file Graph
- ⬜Server:
    - Openai api server
    - ACP server
- ⬜Tree-context-messages
    - message 分叉、缩减、总结

### 功能
- ⬜翻译/划词翻译
- ⬜根据文本/音视频，生成评论/弹幕
- ⬜根据图片内容，提取文本并指定文本在图片上的位置
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

## 编译 
- 
