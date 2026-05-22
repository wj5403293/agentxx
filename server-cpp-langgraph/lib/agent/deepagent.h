#pragma once

#include "agent/config.h"
#include "asio/co_spawn.hpp"
#include "asio/detached.hpp"
#include "asio/io_context.hpp"
#include "neograph/llm/openai_provider.h"
#include "neograph/mcp/client.h"
#include "neograph/neograph.h"
#include "nodes/Toolcall.h"
#include "tools/execute_command.h"
#include "tools/filesystem.h"
#include "tools/get_current_system_datetime.h"
#include "tools/websearch.h"
#include "util/log.h"
#include <format>
#include <iostream>
#include <memory>

namespace agentxx {
class DeepAgent {
protected:
  std::unique_ptr<neograph::graph::GraphEngine> engine = nullptr;
  std::shared_ptr<agentxx::AgentxxConfig_c> config = nullptr;

public:
  DeepAgent(std::shared_ptr<agentxx::AgentxxConfig_c> in_config)
      : config(in_config) {
    assert(nullptr != config);
  }

  void init() {
    assert(config->modelOpenAIBaseUrl.empty() == false);

    std::vector<std::unique_ptr<neograph::Tool>> tools{};
    tools.push_back(
        std::make_unique<agentxx::tools::GetCurrentSystemDateTimeTool>());
    tools.push_back(std::make_unique<agentxx::tools::WebSearchTool>());
    tools.push_back(std::make_unique<agentxx::tools::FetchUrlTool>());
    tools.push_back(std::make_unique<agentxx::tools::FetchUrlMarkdownTool>());
    tools.push_back(std::make_unique<agentxx::tools::FileSystemListFileTool>());
    tools.push_back(std::make_unique<agentxx::tools::FilesystemReadFileTool>());
    tools.push_back(
        std::make_unique<agentxx::tools::FilesystemWriteFileTool>());
    tools.push_back(std::make_unique<agentxx::tools::FilesystemEditFileTool>());
    tools.push_back(std::make_unique<agentxx::tools::FilesystemGlobTool>());
    tools.push_back(std::make_unique<agentxx::tools::ExecuteCommandTool>());

    for (auto &url : config->mcpServerUrls) {
      auto mcp_client = neograph::mcp::MCPClient{url};
      if (mcp_client.initialize(config->agentName)) {
        auto mcp_tools = mcp_client.get_tools();
        XX_LOGD("append mcp tool size: {}", mcp_tools.size());
        tools.insert(tools.end(), std::make_move_iterator(mcp_tools.begin()),
                     std::make_move_iterator(mcp_tools.end()));
      }
    }

    neograph::graph::NodeFactory::instance().register_type(
        std::string{agentxx::nodes::ToolcallNode::defNodeType},
        [](const std::string &name, const neograph::json &,
           const neograph::graph::NodeContext &ctx) {
          return std::make_unique<agentxx::nodes::ToolcallNode>(
              name, ctx,
              [name](neograph::graph::NodeInput &in) {
                return agentxx::nodes::ToolcallNode::
                    defStdoutLogOnToolcallStart(in, name);
              },
              [name](const neograph::graph::NodeInput &in,
                     neograph::graph::NodeOutput &result) {
                return agentxx::nodes::ToolcallNode::defStdoutLogOnToolcallEnd(
                    in, result, name);
              });
        });

    // JSON definition equivalent to the Agent::run() ReAct loop:
    //   __start__ -> llm -> (has_tool_calls ? tools : __end__)
    //                         tools -> llm  (loop back)
    auto definition = neograph::json{
        {"name", "react_agent"},
        {"channels", {{"messages", {{"type", "list"}, {"reducer", "append"}}}}},
        {
            "nodes",
            {
                {"llm", {{"type", "llm_call"}}},
                {
                    "tools",
                    {{"type", agentxx::nodes::ToolcallNode::defNodeType}},
                },
            },
        },
        {
            "edges",
            neograph::json::array({
                {{"from", "__start__"}, {"to", "llm"}},
                {
                    {"from", "llm"},
                    {"type", "conditional"},
                    {"condition", "has_tool_calls"},
                    {"routes", {{"true", "tools"}, {"false", "__end__"}}},
                },
                {{"from", "tools"}, {"to", "llm"}},
            }),
        },
    };

    // Build NodeContext
    neograph::graph::NodeContext ctx;
    ctx.instructions = config->systemPrompt;

    std::vector<neograph::Tool *> tool_ptrs;
    tool_ptrs.reserve(tools.size());
    for (auto &t : tools) {
      tool_ptrs.push_back(t.get());
    }
    ctx.tools = std::move(tool_ptrs);

    neograph::llm::OpenAIProvider::Config provideConfig{
        .api_key = config->modelOpenAIApiKey,
        .base_url = config->modelOpenAIBaseUrl,
        .default_model = config->modelOpenAIModelName,
    };
    ctx.provider = neograph::llm::OpenAIProvider::create_shared(provideConfig);

    auto store = std::make_shared<neograph::graph::InMemoryCheckpointStore>();
    engine = neograph::graph::GraphEngine::compile(definition, ctx, store);
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
                  neograph::json::array(
                      {{{"role", "user"}, {"content", line}}}),
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
    asio::io_context io;
    asio::co_spawn(
        io,
        [this]() -> asio::awaitable<void> { co_return co_await runCliAsync(); },
        asio::detached);
    io.run();
  }

  ~DeepAgent() { engine = nullptr; }
};
}; // namespace agentxx