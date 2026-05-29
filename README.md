## 简介
- c++实现 AI Agent，为了减少内存占用、程序包体积，用于普通用户的手机电脑运行而设计
- 依赖`NeoGraph`，仿照Python著名的 agent 框架`langgraph`，实现常用功能
- 计划实现:
    - 基础模块:
        - ⬜Toolcall添加各种常见功能
            - ✅返回值自动转换字符编码到 utf8
            - ✅filesystem
            - ✅websearch
            - ✅exec_shell
        - ✅Middleware支持
        - ❌MCP支持（Neograph已实现，但暂时使用有问题）
        - ⬜Skill支持
        - ⬜server openai api
        - ⬜tree-context
    - 功能:
        - 翻译/划词翻译
        - 根据文本/音视频，生成评论/弹幕
        - 匹配歌词
        - 操作键鼠
        - 操作live2d模型动作

## 目录结构
- `agent`:
    - c++实现agent
- `skill`:
    - 将加载的 skill 插入到上下文开头
    - 将加载的 skill 插入到上下文末尾
    - 保留加载的skill在上下文 toolcall 位置不动，压缩上下文时整理 skill 到开头（需要确认效果对比整合在一起）

## 编译 
- 
