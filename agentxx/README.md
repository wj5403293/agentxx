# Agentxx
- lumenxx衍生开发的智能体，用于整合lumenxx的各种功能，实现自动处理

## 模块
- apps/agent: langgraph，组合工作流、接入mcp等工具服务
- apps/app: copilotkit，前端ui

## langgraph 更新依赖
```sh
cd apps/agent
.venv/Scripts/activate # 激活 python 环境
# 向`pyproject.toml`添加依赖，修改版本，执行`uv sync`更新
```

## 问题
- 聊天请求失败:
  - 可能是`langgraph`代码执行异常，默认服务端口是7123，可以直接连接 langgraph server 调试`https://smith.langchain.com/studio/?baseUrl=http://localhost:7123`，如果是缺依赖库，[按流程安装](#langgraph-更新依赖)
  - 可能`langgraph`没启动完成，卡在下载依赖包等情况，可以打开vpn，重新启动，知道看见日志输出：
```sh
@repo/agent:dev: INFO:langgraph_api.cli:
@repo/agent:dev: 
@repo/agent:dev:         Welcome to
@repo/agent:dev: 
```
- 日志崩溃，修改`apps/app/next.config.ts`，添加`logging: false`关闭日志打印
- 如果启动后卡住提示等待缓存写入，可以修改`package.json`:
```sh
{
  "scripts": {
    "dev": "turbo run dev --no-cache",
    "dev:app": "turbo run dev --filter='app' --no-cache",
    "dev:agent": "turbo run dev --filter='agent' --no-cache",
    "dev:mcp": "turbo run dev --filter='mcp' --no-cache",
    "build": "turbo run build",
    "lint": "turbo run lint",
    "clean": "turbo run clean"
  },
}
```