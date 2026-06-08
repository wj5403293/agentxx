#pragma once

#include "fmt/format.h"
#include "neograph/neograph.h"
#include "nodes/agentcall.h"
#include "nodes/middleware_handle.h"
#include "nodes/modelcall.h"
#include "nodes/toolcall.h"
#include <format>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace agentxx {
namespace tools {

class SubAgentTaskBase {
protected:
  std::shared_ptr<neograph::graph::GraphEngine> subgraph = nullptr;

public:
  const std::string name;
  const std::string depict;

  SubAgentTaskBase(const std::string &in_subAgentName,
                   const std::string &in_subAgentDepict)
      : name(in_subAgentName), depict(in_subAgentDepict) {}

  virtual std::shared_ptr<neograph::graph::GraphEngine> getSubgraph() = 0;
  virtual ~SubAgentTaskBase() { subgraph = nullptr; }
};

class SubAgentNormalTask : public SubAgentTaskBase {
public:
  SubAgentNormalTask(const std::string &in_subAgentName,
                     const std::string &in_subAgentDepict,
                     const neograph::graph::NodeContext &in_context)
      : SubAgentTaskBase(in_subAgentName, in_subAgentDepict) {
    createSubgraph(in_context);
  }

  void createSubgraph(const neograph::graph::NodeContext &context) {
    if (nullptr == subgraph) {
      auto inner = neograph::graph::GraphEngine::compile(
          defCreateSubGraphDefine(), context);
      subgraph = std::shared_ptr<neograph::graph::GraphEngine>(inner.release());
    }
  }

  std::shared_ptr<neograph::graph::GraphEngine> getSubgraph() override {
    assert(nullptr != subgraph);
    return subgraph;
  }

  inline static neograph::json defCreateSubGraphDefine() {
    return neograph::json{
        {"name", "xx_SubAgentTask"},
        {
            "channels",
            {
                {
                    "messages",
                    {{"type", "list"}, {"reducer", "append"}},
                },
            },
        },
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
                    "tools",
                    {{
                        "type",
                        agentxx::nodes::MiddlewareWrapToolcallNode::defNodeType,
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
            },
        },
        {
            "edges",
            neograph::json::array({
                {{"from", "__start__"}, {"to", "llm"}},
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
  }
};

class SubAgentManagerTool : public neograph::AsyncTool {
  const std::string nodeName;

public:
  std::map<std::string, std::shared_ptr<SubAgentTaskBase>> subAgentList{};

  explicit SubAgentManagerTool(const std::string &in_nodeName)
      : nodeName(in_nodeName) {}

  std::string get_name() const override { return "subagent_switch"; }

  neograph::ChatTool get_definition() const override {
    auto subagentNameList = std::vector<std::string>{};
    std::ostringstream subagentNameDepict;
    for (const auto &item : subAgentList) {
      subagentNameList.push_back(item.first);
      subagentNameDepict << fmt::format("`{}`: {}\n", item.first,
                                        item.second->depict);
    }

    return {
        "subagent_switch",
        "Switch a isolation messages context sub-agent to exec.",
        neograph::json{
            {"type", "object"},
            {
                "properties",
                {
                    {
                        "subagent",
                        {
                            {"type", "string"},
                            {"enum", neograph::json{subagentNameList}},
                            {
                                "description",
                                fmt::format("Target sub-agent name.\n{}",
                                            subagentNameDepict.str()),
                            },
                        },
                    },
                    {
                        "system_prompt",
                        {
                            {"type", "string"},
                            {"description", "Sub-agent system prompt"},
                        },
                    },
                    {
                        "message",
                        {
                            {"type", "string"},
                            {"description", "Task content as a user message"},
                        },
                    },
                },
            },
            {
                "required",
                neograph::json::array({"subagent", "system_prompt", "message"}),
            },
        },
    };
  }

  asio::awaitable<std::string>
  execute_async(const neograph::json &arguments) override {
    // 这里无法修改跳转到指定节点，因此只接收 llm 节点决定启用哪个 subagent
    // 的参数，转由 [SubAgentMiddlewareHandle::onToolcallEndFunc] 处理
    auto subagentName = arguments.value("subagent", std::string{});
    if (subagentName.empty()) {
      co_return R"({"error":"Arg `subagent` is empty"})";
    }
    auto system_prompt = arguments.value("system_prompt", std::string{});
    if (system_prompt.empty()) {
      co_return R"({"error":"Arg `system_prompt` is empty"})";
    }
    auto message = arguments.value("message", std::string{});
    if (message.empty()) {
      co_return R"({"error":"Arg `message` is empty"})";
    }

    auto subagentIt = subAgentList.find(subagentName);
    if (subagentIt == subAgentList.end() || nullptr == subagentIt->second) {
      std::ostringstream subagentNames;
      bool isFirst = true;
      for (const auto &item : subAgentList) {
        subagentNames << item.first;
        if (false == isFirst) {
          subagentNames << ",";
        }
      }
      co_return fmt::format(
          R"({{"error":"Arg `subagent` is not one of [{}]"}})",
          subagentNames.str());
    }

    auto subagent = subagentIt->second;
    assert(nullptr != subagent->getSubgraph());

    try {
      neograph::graph::RunConfig cfg{
          .thread_id = "session",
          .input = {{
              "messages",
              neograph::json::array({
                  {
                      {"role", "system"},
                      {"content", system_prompt},
                  },
                  {
                      {"role", "user"},
                      {"content", message},
                  },
              }),
          }},
          .resume_if_exists = true,
      };

      std::ostringstream oss;
      XX_LOGD("    ## Subagent - {}", subagent->name);
      auto result = co_await subagent->getSubgraph()->run_stream_async(
          cfg, [&oss](const neograph::graph::GraphEvent &event) {
            switch (event.type) {
            case neograph::graph::GraphEvent::Type::NODE_START:
            case neograph::graph::GraphEvent::Type::NODE_END:
              break;
            case neograph::graph::GraphEvent::Type::LLM_TOKEN: {
              const auto token = event.data.get<std::string>();
              oss << token;
#if IS_DEBUG_D
              std::cout << token << std::flush;
#endif
            } break;
            case neograph::graph::GraphEvent::Type::CHANNEL_WRITE:
            case neograph::graph::GraphEvent::Type::INTERRUPT:
            case neograph::graph::GraphEvent::Type::ERROR:
              break;
            }
          });
      co_return oss.str();
    } catch (const std::exception &e) {
      co_return fmt::format(R"({{"error": "Sub-agent Response failed: {}"}})",
                            e.what());
    }
  }
};

}; // namespace tools
}; // namespace agentxx