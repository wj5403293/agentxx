#pragma once

#include "agent/config.h"
#include "asio/co_spawn.hpp"
#include "asio/detached.hpp"
#include "asio/io_context.hpp"
#include "neograph/llm/openai_provider.h"
#include "neograph/mcp/client.h"
#include "neograph/neograph.h"
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

    neograph::llm::OpenAIProvider::Config provideConfig{
        .api_key = config->modelOpenAIApiKey,
        .base_url = config->modelOpenAIBaseUrl,
        .default_model = config->modelOpenAIModelName,
    };
    auto provider = neograph::llm::OpenAIProvider::create_shared(provideConfig);

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

    for (auto &url : config->mcpServerUrls) {
      auto mcp_client = neograph::mcp::MCPClient{url};
      if (mcp_client.initialize(config->agentName)) {
        auto mcp_tools = mcp_client.get_tools();
        XX_LOGD("append mcp tool size: {}", mcp_tools.size());
        tools.insert(tools.end(), std::make_move_iterator(mcp_tools.begin()),
                     std::make_move_iterator(mcp_tools.end()));
      }
    }

    engine = neograph::graph::create_react_graph(provider, std::move(tools),
                                                 config->systemPrompt);
    auto store = std::make_shared<neograph::graph::InMemoryCheckpointStore>();
    engine->set_checkpoint_store(store);
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