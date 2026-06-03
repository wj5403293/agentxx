## TODO
- 文本暂存 tool，允许模型主动调用，封存上下文中的指定数据到内存，只在上下文中保留id和缩略描述

## LOG
- 2026/06/03
    - 引入 `asio::stream_file` 等支持异步文件读写
- 2026/06/02
    - 添加 middleware，拦截 `agentCall`、`modelCall`、`toolCall`
    - 添加 skill 解析
- 2026/05/22
    - `toolcall`:
        - 支持`filesystem`、`websearch`、`execute_command`、`string_util`
        - 添加 ToolcallNode，优化 toolcall 捕获异常、输出调用日志
