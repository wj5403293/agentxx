#include "neograph/llm/openai_provider.h"
#include "neograph/neograph.h"
#include <iostream>

namespace agentxx {
class DeepAgent {
protected:
  std::unique_ptr<neograph::graph::GraphEngine> engine = nullptr;

public:
  DeepAgent() {}

  void init() {
    neograph::llm::OpenAIProvider::Config cfg{
        .api_key = "EMPTY",
        .base_url = "http://172.29.48.1:7070",
        .default_model = "agentxx",
    };
    auto provider = neograph::llm::OpenAIProvider::create_shared(cfg);

    neograph::graph::NodeContext ctx{
        .provider = provider,
        .model = "agentxx",
        .instructions = "Reply in one short sentence.",
    };

    neograph::json definition{
        {"name", "chatbot"},
        {"channels", {{"messages", {{"reducer", "append"}}}}},
        {"nodes", {{"llm", {{"type", "llm_call"}}}}},
        {"edges",
         neograph::json::array({{{"from", "__start__"}, {"to", "llm"}},
                                {{"from", "llm"}, {"to", "__end__"}}})}};
    auto store = std::make_shared<neograph::graph::InMemoryCheckpointStore>();
    engine = neograph::graph::GraphEngine::compile(definition, ctx, store);
  }

  void runCli() {
    std::cout << ">>> ";
    for (std::string line; std::getline(std::cin, line);) {
      if (false == line.empty()) {
        neograph::graph::RunConfig cfg{
            .thread_id = "session-1",
            .input = {{"messages",
                       neograph::json::array(
                           {{{"role", "user"}, {"content", line}}})}},
            .resume_if_exists = true,
        };

        std::cout << "Agentxx: ";
        auto result = engine->run_stream(
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
      }
      std::cout << "\n>>> ";
    }
  }

  ~DeepAgent() { engine = nullptr; }
};
}; // namespace agentxx