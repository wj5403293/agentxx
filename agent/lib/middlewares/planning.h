#pragma once

#include "asio/io_context.hpp"
#include "fmt/format.h"
#include "middlewares/middleware.h"
#include "util/string_util.h"
#include "yaml-cpp/yaml.h"
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <neograph/llm/rate_limited_provider.h>
#include <neograph/llm/schema_provider.h>
#include <neograph/neograph.h>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace agentxx {
namespace middleware {

class PlanningMiddlewareState_c : public BaseMiddlewareState_c {
public:
  /// <thread_id, todoListJson>
  /// [会话独立] 任务规划列表，由 todolist tool 读写
  std::map<std::string, neograph::json> plannings{};

  PlanningMiddlewareState_c() {}
};

class PlanningMiddlewareHandle
    : public BaseMiddlewareHandle<PlanningMiddlewareState_c> {
protected:
  inline static constexpr std::string_view defTodolistPromptTemplate =
      std::string_view{R"_(

## Task Plan

### Strategic Overview (State Diagram)
The state diagram below is your overall roadmap. It shows all major phases,
their dependencies, and error recovery paths. Use it to understand where you
are in the workflow and decide what to do next.

```mermaid
{}
```

### Tactical Execution (Near-term Tasks)
The items below are what you are actively working on RIGHT NOW and what comes
immediately next. Focus on completing the `in_progress` item before moving on.

{}

### Instructions
- When you start working on a task, mark it `in_progress`
- When a task succeeds, mark it `completed` and transition to the next phase
- When a task fails, mark it `failed` and plan a recovery path in the diagram
- Update the plan via `todolist_write_todos` whenever task status changes
- Keep `todos` short: only current + next, not the whole plan
)_"};

  std::string formatTodolistPrompt(const neograph::json &planStore) {
    auto mermaid = planStore.value("mermaid", std::string{});
    if (mermaid.empty()) {
      return "";
    }

    std::string todosText;
    if (planStore.contains("todos") && planStore["todos"].is_array()) {
      std::ostringstream oss;
      for (const auto &item : planStore["todos"]) {
        auto state = item.value("state", std::string{"unknown"});
        auto content = item.value("content", std::string{});
        auto summary = item.value("summary", std::string{});
        oss << fmt::format("- [{}] {}", state, content);
        if (!summary.empty()) {
          oss << fmt::format("\n  Summary: {}", summary);
        }
        oss << "\n";
      }
      todosText = oss.str();
    } else {
      todosText = "(no detailed task items)";
    }

    return fmt::format(defTodolistPromptTemplate, mermaid, todosText);
  }

public:
  PlanningMiddlewareHandle(
      std::weak_ptr<MiddlewareWarpHandleContext> in_handleContext)
      : BaseMiddlewareHandle<PlanningMiddlewareState_c>(
            "PlanningMiddlewareHandle", in_handleContext) {}

  asio::awaitable<void>
  onAgentcallStartFunc(neograph::graph::NodeInput &in) override {
    co_return;
  }

  asio::awaitable<void>
  onAgentcallEndFunc(const neograph::graph::NodeInput &in,
                     neograph::graph::NodeOutput &result) override {
    co_return;
  }

  asio::awaitable<void>
  onModelcallStartFunc(neograph::graph::NodeInput &in) override {
    auto todolistState = co_await getStateItem(in.ctx.thread_id);

    auto it = todolistState->plannings.find(in.ctx.thread_id);
    if (it == todolistState->plannings.end()) {
      co_return;
    }

    auto prompt = formatTodolistPrompt(it->second);
    if (prompt.empty()) {
      co_return;
    }

    auto handleContextPtr = handleContext.lock();
    if (nullptr == handleContextPtr) {
      co_return;
    }
    auto &appendSystemMsgList =
        handleContextPtr->getGraphDataItemValue<std::vector<std::string>>(
            in.ctx.thread_id, agentxx::middleware::MiddlewareWarpHandleContext::
                                  graphDataKey_systemMessage);
    appendSystemMsgList.push_back(prompt);
    co_return;
  }

  asio::awaitable<void>
  onModelcallEndFunc(const neograph::graph::NodeInput &in,
                     neograph::graph::NodeOutput &result) override {
    co_return;
  }

  asio::awaitable<void>
  onToolcallStartFunc(neograph::graph::NodeInput &in) override {
    co_return;
  }

  asio::awaitable<void>
  onToolcallEndFunc(const neograph::graph::NodeInput &in,
                    neograph::graph::NodeOutput &result) override {
    co_return;
  }
};
} // namespace middleware
} // namespace agentxx