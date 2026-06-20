## TODO
- hyperscan 匹配 \w 时包含了换行符

## LOG
- 2026/06/20
    - 支持主动中断、恢复执行
    - 修复消息上下文拼接问题
- 2026/06/18
    - `asio`改用`Boost.asio`
    - `execute_command`支持 `Boost.process` 协程异步执行外部命令
- 2026/06/17
    - `share_store`支持分页读取
    - 优化 tool 描述，调整参数描述位置
- 2026/06/08
    - 新增支持 `sub-agent`
- 2026/06/03
    - 引入 `asio::stream_file` 等支持异步文件读写
- 2026/06/02
    - 添加 middleware，拦截 `agentCall`、`modelCall`、`toolCall`
    - 添加 skill 解析
- 2026/05/22
    - `toolcall`:
        - 支持`filesystem`、`websearch`、`execute_command`、`string_util`
        - 添加 ToolcallNode，优化 toolcall 捕获异常、输出调用日志
