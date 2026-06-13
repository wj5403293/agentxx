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
    auto handleContextPtr = handleContext.lock();
    auto appendSystemPromptList =
        handleContextPtr->getGraphDataItemValue<std::vector<std::string>>(
            in.ctx.thread_id, agentxx::middleware::MiddlewareWarpHandleContext::
                                  graphDataKey_systemMessage);
    appendSystemPromptList.push_back(
        R"(
## Planning

You have access to the `planning_write` tool to help you manage and plan complex objectives.
Use this tool for complex objectives to ensure that you are tracking each necessary step.
This tool is very helpful for planning complex objectives, and for breaking down these larger complex objectives into smaller steps.

It is critical that you mark todos as completed as soon as you are done with a step. Do not batch up multiple steps before marking them as completed.
For simple objectives that only require a few steps, it is better to just complete the objective directly and NOT use this tool.
Writing roadmap and todos takes time and tokens, use it when it is helpful for managing complex many-step problems! But not for simple few-step requests.

### Important To-Do List Usage Notes to Remember

- The `planning_write` tool should never be called multiple times in parallel.
- Don't be afraid to revise the To-Do list as you go. New information may reveal new tasks that need to be done, or old tasks that are irrelevant.

### Finishing a task

When you finish all work, write your final answer in the message AFTER your last `planning_write` call — not in the same turn as that call. Start the final message with the substantive content the user asked for — the data, computation, summary, or analysis. The user wants the result, not confirmation that the work is done."
)");
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