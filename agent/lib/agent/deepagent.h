#pragma once

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
#include "util/log.h"
#include <format>
#include <iostream>
#include <memory>

namespace agentxx {

class DeepAgent {
protected:
  /// 主协程调度器
  /// - 不要在同线程中传递到 [runCliAsync]
  /// 内使用，因为[engine->run_stream_async]会启动其他 io_context， 交替 ioCtx
  /// 的话会导致互相等待，进而卡住
  /// - 某个异步函数需要 io_context 时，可以通过 `co_await
  /// asio::this_coro::executor` 获取当前异步函数运行时绑定的 io_context
  std::shared_ptr<asio::io_context> ioCtx = nullptr;
  std::unique_ptr<neograph::graph::GraphEngine> engine = nullptr;
  std::shared_ptr<agentxx::AgentxxConfig_c> config = nullptr;
  std::shared_ptr<agentxx::middleware::MiddlewareWarpHandleContext>
      middlewareHandleContext = nullptr;

public:
  DeepAgent(std::shared_ptr<agentxx::AgentxxConfig_c> in_config)
      : config(in_config) {
    assert(nullptr != config);
    assert(config->modelOpenAIBaseUrl.empty() == false);
    ioCtx = std::make_shared<asio::io_context>();
  }

  void init() {

    /// Toolcall
    std::vector<std::unique_ptr<neograph::Tool>> tools{};
    {
      tools.push_back(
          std::make_unique<agentxx::tools::GetCurrentDateTimeTool>());

      tools.push_back(std::make_unique<agentxx::tools::WebSearchTool>());
      tools.push_back(std::make_unique<agentxx::tools::FetchUrlTool>());
      tools.push_back(std::make_unique<agentxx::tools::FetchUrlMarkdownTool>());

      tools.push_back(
          std::make_unique<agentxx::tools::FileSystemListFileTool>());
      tools.push_back(
          std::make_unique<agentxx::tools::FilesystemReadTextFileTool>());
      tools.push_back(
          std::make_unique<agentxx::tools::FilesystemReadBinaryFileTool>());
      tools.push_back(
          std::make_unique<agentxx::tools::FilesystemWriteFileTool>());
      tools.push_back(
          std::make_unique<agentxx::tools::FilesystemEditFileTool>());
      tools.push_back(std::make_unique<agentxx::tools::FilesystemGlobTool>());

      tools.push_back(
          std::make_unique<agentxx::tools::StringHtml2MarkdownTool>());
      tools.push_back(std::make_unique<agentxx::tools::StringRegexpTool>());

#if IS_WIN_D
      tools.push_back(
          std::make_unique<agentxx::tools::ExecuteWindowsCommandTool>());
#elif IS_LINUX_D
      tools.push_back(
          std::make_unique<agentxx::tools::ExecuteLinuxCommandTool>());
      if (agentxx::util::isRunningInWSL()) {
        tools.push_back(
            std::make_unique<agentxx::tools::ExecuteWindowsCommandTool>());
      }
#endif
    }

    {
      /// MCP
      for (auto &url : config->mcpServerUrls) {
        auto mcpClient = neograph::mcp::MCPClient{url};
        if (mcpClient.initialize(config->agentName)) {
          auto mcpTools = mcpClient.get_tools();
          XX_LOGD("append mcp tool size: {}", mcpTools.size());
          tools.insert(tools.end(), std::make_move_iterator(mcpTools.begin()),
                       std::make_move_iterator(mcpTools.end()));
        }
      }
    }

    {
      /// middleware
      middlewareHandleContext =
          std::make_shared<agentxx::middleware::MiddlewareWarpHandleContext>();

      {
        auto skillMiddleware =
            std::make_unique<agentxx::middleware::SkillMiddlewareHandle>(
                std::set<std::string>{
                    "/home/coolight/program/agentxx/isolation/skills/"},
                middlewareHandleContext);
        skillMiddleware->toolcalls.push_back(
            std::make_unique<agentxx::tools::SkillTool>());
        middlewareHandleContext->handles.push_back(std::move(skillMiddleware));
      }

      /// Toolcall  应当作为最后一层
      middlewareHandleContext->handles.push_back(
          std::make_unique<agentxx::middleware::MiddlewareWarpHandle<
              agentxx::middleware::BaseMiddlewareState_c>>(
              "toolcall_log", middlewareHandleContext,
              (agentxx::middleware::onGraphNodeBeforeCallFunc) nullptr,
              (agentxx::middleware::onGraphNodeAfterCallFunc) nullptr,
              (agentxx::middleware::onGraphNodeBeforeCallFunc) nullptr,
              (agentxx::middleware::onGraphNodeAfterCallFunc) nullptr,
              [](neograph::graph::NodeInput &in) -> asio::awaitable<void> {
                return agentxx::nodes::MiddlewareWrapToolcallNode::
                    defStdoutLogOnToolcallStart(in);
              },
              [](const neograph::graph::NodeInput &in,
                 neograph::graph::NodeOutput &result) {
                return agentxx::nodes::MiddlewareWrapToolcallNode::
                    defStdoutLogOnToolcallEnd(in, result);
              }));

      /// 添加 tools
      for (auto &item : middlewareHandleContext->handles) {
        if (false == item->toolcalls.empty()) {
          tools.insert(tools.end(),
                       std::make_move_iterator(item->toolcalls.begin()),
                       std::make_move_iterator(item->toolcalls.end()));
        }
      }
    }

    // Build NodeContext
    neograph::graph::NodeContext nodeContext{};
    nodeContext.instructions = config->systemPrompt;

    std::vector<neograph::Tool *> toolPtrs;
    toolPtrs.reserve(tools.size());
    for (auto &t : tools) {
      toolPtrs.push_back(t.get());
    }
    nodeContext.tools = std::move(toolPtrs);

    neograph::llm::OpenAIProvider::Config provideConfig{
        .api_key = config->modelOpenAIApiKey,
        .base_url = config->modelOpenAIBaseUrl,
        .default_model = config->modelOpenAIModelName,
    };
    nodeContext.provider =
        neograph::llm::OpenAIProvider::create_shared(provideConfig);

    auto store = std::make_shared<neograph::graph::InMemoryCheckpointStore>();

    // JSON definition equivalent to the Agent::run() ReAct loop:
    //   __start__ -> llm -> (has_tool_calls ? tools : __end__)
    //                         tools -> llm  (loop back)
    auto graphDefinition = neograph::json{
        {"name", config->agentName},
        {"channels", {{"messages", {{"type", "list"}, {"reducer", "append"}}}}},
        {
            "nodes",
            {
                {
                    "agent_start",
                    {{
                        "type",
                        agentxx::nodes::MiddlewareWrapAgentStartCallNode::
                            defNodeType,
                    }},
                },
                {
                    "agent_end",
                    {{
                        "type",
                        agentxx::nodes::MiddlewareWrapAgentEndCallNode::
                            defNodeType,
                    }},
                },
                {
                    "llm",
                    {{
                        "type",
                        agentxx::nodes::MiddlewareWrapModelCallNode::
                            defNodeType,
                    }},
                },
                {
                    "tools",
                    {{
                        "type",
                        agentxx::nodes::MiddlewareWrapToolcallNode::defNodeType,
                    }},
                },
            },
        },
        {
            "edges",
            neograph::json::array({
                {{"from", "__start__"}, {"to", "agent_start"}},
                {{"from", "agent_start"}, {"to", "llm"}},
                {
                    {"from", "llm"},
                    {"type", "conditional"},
                    {"condition", "has_tool_calls"},
                    {"routes", {{"true", "tools"}, {"false", "agent_end"}}},
                },
                {{"from", "tools"}, {"to", "llm"}},
                {{"from", "agent_end"}, {"to", "__end__"}},
            }),
        },
    };

    {
      /// register Node
      neograph::graph::NodeFactory::instance().register_type(
          std::string{
              agentxx::nodes::MiddlewareWrapAgentStartCallNode::defNodeType},
          [handleCtx = middlewareHandleContext](
              const std::string &name, const neograph::json &,
              const neograph::graph::NodeContext &ctx) {
            return std::make_unique<
                agentxx::nodes::MiddlewareWrapAgentStartCallNode>(name, ctx,
                                                                  handleCtx);
          });
      neograph::graph::NodeFactory::instance().register_type(
          std::string{
              agentxx::nodes::MiddlewareWrapAgentEndCallNode::defNodeType},
          [handleCtx = middlewareHandleContext](
              const std::string &name, const neograph::json &,
              const neograph::graph::NodeContext &ctx) {
            return std::make_unique<
                agentxx::nodes::MiddlewareWrapAgentEndCallNode>(name, ctx,
                                                                handleCtx);
          });
      neograph::graph::NodeFactory::instance().register_type(
          std::string{agentxx::nodes::MiddlewareWrapToolcallNode::defNodeType},
          [handleCtx = middlewareHandleContext](
              const std::string &name, const neograph::json &,
              const neograph::graph::NodeContext &ctx) {
            return std::make_unique<agentxx::nodes::MiddlewareWrapToolcallNode>(
                name, ctx, handleCtx);
          });
      neograph::graph::NodeFactory::instance().register_type(
          std::string{agentxx::nodes::MiddlewareWrapModelCallNode::defNodeType},
          [handleCtx = middlewareHandleContext](
              const std::string &name, const neograph::json &,
              const neograph::graph::NodeContext &ctx) {
            return std::make_unique<
                agentxx::nodes::MiddlewareWrapModelCallNode>(name, ctx,
                                                             handleCtx);
          });
    }

    engine = neograph::graph::GraphEngine::compile(graphDefinition, nodeContext,
                                                   store);
    engine->own_tools(std::move(tools));
  }

  asio::awaitable<void> runCliAsync() {
    std::cout << ">>> " << std::flush;

    for (std::string line; std::getline(std::cin, line);) {
      if (false == line.empty()) {
        try {
          neograph::graph::RunConfig cfg{
              .thread_id = "session",
              .input = {{
                  "messages",
                  neograph::json::array({{
                      {"role", "user"},
                      {"content", line},
                  }}),
              }},
              .resume_if_exists = true,
          };

          std::cout << config->agentNameView << ": " << std::flush;
          auto result = co_await engine->run_stream_async(
              cfg, [](const neograph::graph::GraphEvent &event) {
                switch (event.type) {
                case neograph::graph::GraphEvent::Type::NODE_START:
                case neograph::graph::GraphEvent::Type::NODE_END:
                  break;
                case neograph::graph::GraphEvent::Type::LLM_TOKEN: {
                  std::cout << event.data.get<std::string>() << std::flush;
                } break;
                case neograph::graph::GraphEvent::Type::CHANNEL_WRITE:
                case neograph::graph::GraphEvent::Type::INTERRUPT:
                case neograph::graph::GraphEvent::Type::ERROR:
                  break;
                }
              });
        } catch (const std::exception &e) {
          XX_LOGE(R"({{"error": "Agent Response failed: {}"}})", e.what());
        }
      }
      std::cout << "\n\n>>> ";
    }
  }

  void runCli() {
    asio::co_spawn(
        *ioCtx,
        [this]() -> asio::awaitable<void> { co_return co_await runCliAsync(); },
        asio::detached);
    ioCtx->run();
  }

  ~DeepAgent() { engine = nullptr; }
};
}; // namespace agentxx