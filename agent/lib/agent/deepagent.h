#pragma once

#include "agent/config.h"
#include "asio/co_spawn.hpp"
#include "asio/detached.hpp"
#include "asio/io_context.hpp"
#include "middlewares/planning.h"
#include "middlewares/skill.h"
#include "middlewares/summarization.h"
#include "neograph/llm/openai_provider.h"
#include "neograph/mcp/client.h"
#include "neograph/neograph.h"
#include "nodes/agentcall.h"
#include "nodes/modelcall.h"
#include "nodes/toolcall.h"
#include "tools/execute_command.h"
#include "tools/filesystem.h"
#include "tools/get_current_datetime.h"
#include "tools/planning.h"
#include "tools/skill.h"
#include "tools/string.h"
#include "tools/sub_agent.h"
#include "tools/temp_store.h"
#include "tools/tool_skill_search.h"
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
    neograph::llm::OpenAIProvider::Config provideConfig{
        .api_key = config->modelOpenAIApiKey,
        .base_url = config->modelOpenAIBaseUrl,
        .default_model = config->modelOpenAIModelName,
    };

    {
      /// register Node
      neograph::graph::NodeFactory::instance().register_type(
          std::string{agentxx::nodes::AgentStartCallWrapNode::defNodeType},
          [this](const std::string &name, const neograph::json &,
                 const neograph::graph::NodeContext &ctx) {
            return std::make_unique<agentxx::nodes::AgentStartCallWrapNode>(
                name, middlewareHandleContext);
          });
      neograph::graph::NodeFactory::instance().register_type(
          std::string{
              agentxx::nodes::MiddlewareWrapAgentEndCallNode::defNodeType},
          [this](const std::string &name, const neograph::json &,
                 const neograph::graph::NodeContext &ctx) {
            return std::make_unique<
                agentxx::nodes::MiddlewareWrapAgentEndCallNode>(
                name, middlewareHandleContext);
          });
      neograph::graph::NodeFactory::instance().register_type(
          std::string{agentxx::nodes::ModelCallWrapNode::defNodeType},
          [this](const std::string &name, const neograph::json &,
                 const neograph::graph::NodeContext &ctx) {
            return std::make_unique<agentxx::nodes::ModelCallWrapNode>(
                name, ctx, middlewareHandleContext);
          });
      neograph::graph::NodeFactory::instance().register_type(
          std::string{agentxx::nodes::ToolcallWrapNode::defNodeType},
          [this](const std::string &name, const neograph::json &,
                 const neograph::graph::NodeContext &ctx) {
            return std::make_unique<agentxx::nodes::ToolcallWrapNode>(
                name, ctx, middlewareHandleContext);
          });
    }

    /// middleware
    middlewareHandleContext =
        std::make_shared<agentxx::middleware::MiddlewareWarpHandleContext>();
    auto subagentManagerTool =
        std::make_unique<agentxx::tools::SubAgentManagerTool>(
            "subagent_manager");
    {
      {
        auto skillMiddleware =
            std::make_shared<agentxx::middleware::SkillMiddlewareHandle>(
                config->skillDirPaths, middlewareHandleContext);
        // skillMiddleware->toolcalls.push_back(
        //     std::make_unique<agentxx::tools::SkillTool>());
        middlewareHandleContext->handles.push_back(skillMiddleware);
      }

      {
        auto summarizationMiddleware = std::make_shared<
            agentxx::middleware::SummarizationMiddlewareHandle>(
            subagentManagerTool.get(), "subagent_task", middlewareHandleContext,
            256 * 1024 * 1024);
        middlewareHandleContext->handles.push_back(summarizationMiddleware);
      }

      {
        auto todolistMiddleware =
            std::make_shared<agentxx::middleware::PlanningMiddlewareHandle>(
                middlewareHandleContext);
        todolistMiddleware->toolcalls.push_back(
            std::make_unique<agentxx::tools::WritePlanningTool>(
                todolistMiddleware));
        middlewareHandleContext->handles.push_back(todolistMiddleware);
      }

      /// Toolcall  应当作为最后一层
      middlewareHandleContext->handles.push_back(
          std::make_shared<agentxx::middleware::MiddlewareWarpHandle<
              agentxx::middleware::BaseMiddlewareState_c>>(
              "toolcall_log", middlewareHandleContext,
              (agentxx::middleware::onGraphNodeBeforeCallFunc) nullptr,
              (agentxx::middleware::onGraphNodeAfterCallFunc) nullptr,
              (agentxx::middleware::onGraphNodeBeforeCallFunc) nullptr,
              (agentxx::middleware::onGraphNodeAfterCallFunc) nullptr,
              [](neograph::graph::NodeInput &in) -> asio::awaitable<void> {
                return agentxx::nodes::ToolcallWrapNode::
                    defStdoutLogOnToolcallStart(in);
              },
              [](const neograph::graph::NodeInput &in,
                 neograph::graph::NodeOutput &result) {
                return agentxx::nodes::ToolcallWrapNode::
                    defStdoutLogOnToolcallEnd(in, result);
              }));
    }

    /// Toolcall
    std::vector<std::unique_ptr<neograph::Tool>> tools{};
    {
      /// middleware tools
      for (auto &item : middlewareHandleContext->handles) {
        if (false == item->toolcalls.empty()) {
          tools.insert(tools.end(),
                       std::make_move_iterator(item->toolcalls.begin()),
                       std::make_move_iterator(item->toolcalls.end()));
        }
      }
    }
    {
      /// MCP tool
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
      tools.push_back(std::make_unique<agentxx::tools::TempKVStoreTool>(
          middlewareHandleContext));
      tools.push_back(
          std::make_unique<agentxx::tools::FileSystemListFileTool>());
      tools.push_back(
          std::make_unique<agentxx::tools::FilesystemReadTextFileTool>());
      tools.push_back(
          std::make_unique<agentxx::tools::FilesystemReadBinaryFileTool>());
      tools.push_back(
          std::make_unique<agentxx::tools::FilesystemWriteFileTool>());
      tools.push_back(
          std::make_unique<agentxx::tools::FilesystemEditTextFileTool>());
      tools.push_back(std::make_unique<agentxx::tools::FilesystemGlobTool>());
      tools.push_back(std::make_unique<agentxx::tools::FilesystemGrepTool>());

      tools.push_back(
          std::make_unique<agentxx::tools::StringHtml2MarkdownTool>());
      tools.push_back(std::make_unique<agentxx::tools::StringRegexpTool>());

      tools.push_back(std::make_unique<agentxx::tools::WebSearchTool>());
      tools.push_back(std::make_unique<agentxx::tools::FetchUrlTool>());
      tools.push_back(std::make_unique<agentxx::tools::FetchUrlMarkdownTool>());

      tools.push_back(
          std::make_unique<agentxx::tools::GetCurrentDateTimeTool>());

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

      {
        // subagent
        // {
        //   // tool_skill_search
        //   // - 复制 除了 subagent 的所有 tool/mcp tool/skill 组合上下文到
        //   // subagent 中 根据需求分析加载/使用的 tool/skill
        //   neograph::graph::NodeContext nodeContext{};
        //   nodeContext.instructions = "";
        //   nodeContext.provider =
        //       neograph::llm::OpenAIProvider::create_shared(provideConfig);

        //   // 收集延迟加载的 tool 信息
        //   std::vector<agentxx::tools::ToolSkillSearchTool::DelayToolInfo>
        //       delayToolInfos;
        //   for (auto &t : tools) {
        //     auto *xxTool = dynamic_cast<agentxx::tools::XXToolBase
        //     *>(t.get()); if (xxTool && xxTool->isDelayLoad) {
        //       auto def = xxTool->get_definition();
        //       delayToolInfos.push_back({xxTool->get_name(),
        //       def.description});
        //     }
        //   }

        //   // 给子 agent 提供文件系统 tool，用于搜索和读取 SKILL.md
        //   std::vector<neograph::Tool *> searchToolPtrs;
        //   searchToolPtrs.reserve(tools.size());
        //   for (auto &t : tools) {
        //     const auto &name = t->get_name();
        //     if (name == "filesystem_glob" || name == "filesystem_listfile" ||
        //         name == "filesystem_read_text_file" ||
        //         name == "filesystem_grep") {
        //       searchToolPtrs.push_back(t.get());
        //     }
        //   }
        //   nodeContext.tools = std::move(searchToolPtrs);

        //   subagentManagerTool->subAgentList.insert(std::make_pair(
        //       "tool_skill_search",
        //       std::make_shared<agentxx::tools::ToolSkillSearchTool>(
        //           nodeContext, delayToolInfos, config->skillDirPaths,
        //           middlewareHandleContext)));
        // }
        {
          // subagent_task
          neograph::graph::NodeContext nodeContext{};
          nodeContext.instructions = "";
          nodeContext.provider =
              neograph::llm::OpenAIProvider::create_shared(provideConfig);

          /// 复制 tool
          std::vector<neograph::Tool *> toolPtrs;
          toolPtrs.reserve(tools.size());
          for (auto &t : tools) {
            toolPtrs.push_back(t.get());
          }
          nodeContext.tools = std::move(toolPtrs);

          const auto nodeName = std::string{"subagent_task"};

          subagentManagerTool->subAgentList.insert(std::make_pair(
              nodeName, std::make_shared<agentxx::tools::SubAgentNormalTask>(
                            nodeName,
                            "Create a isolation messages context sub agent to "
                            "exec. (need system prompt)",
                            nodeContext)));
        }

        tools.push_back(std::move(subagentManagerTool));
      }
    }

    /// Main Agent
    neograph::graph::NodeContext nodeContext{};
    nodeContext.instructions = config->systemPrompt;
    nodeContext.provider =
        neograph::llm::OpenAIProvider::create_shared(provideConfig);

    std::vector<neograph::Tool *> toolPtrs;
    toolPtrs.reserve(tools.size());
    for (auto &t : tools) {
      toolPtrs.push_back(t.get());
    }
    nodeContext.tools = std::move(toolPtrs);

    auto store = std::make_shared<neograph::graph::InMemoryCheckpointStore>();

    // JSON definition equivalent to the Agent::run() ReAct loop:
    //                 ------- sub_agent_task <--- toolcall/sub_agent_task
    //                 |                            |
    //                 |<---------------------------|
    //                 |                            |
    //                 v                            |
    //  __start__  -> llm ->  has_tool_calls  ->  tools
    //                               |
    //                               v
    //                            __end__
    auto graphDefinition = neograph::json{
        {"name", config->agentName},
        {
            "channels",
            {
                {"messages", {{"type", "list"}, {"reducer", "append"}}},
                agentxx::middleware::SummarizationMiddlewareHandle::
                    defChannelDefine(),
            },
        },
        {
            "nodes",
            {
                {
                    "agent_start",
                    {{
                        "type",
                        agentxx::nodes::AgentStartCallWrapNode::defNodeType,
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
                    "tools",
                    {{
                        "type",
                        agentxx::nodes::ToolcallWrapNode::defNodeType,
                    }},
                },
                {
                    "llm",
                    {{
                        "type",
                        agentxx::nodes::ModelCallWrapNode::defNodeType,
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