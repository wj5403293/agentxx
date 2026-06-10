#pragma once

#include "middlewares/todo_list.h"
#include <filesystem>
#include <format>
#include <iostream>
#include <map>
#include <memory>
#include <neograph/llm/rate_limited_provider.h>
#include <neograph/llm/schema_provider.h>
#include <neograph/neograph.h>
#include <sstream>
#include <string>
#include <vector>

namespace agentxx {
namespace tools {

/// 规划任务，提高模型在长流程中的注意力
// Task planning
// The harness provides a tool that agents can use to maintain a
// structured task list. Features:
//   - Track multiple tasks with statuses ('pending', 'in_progress',
//   'completed', 'faild')
//   - Persisted in agent state per thread
//   - Helps agent organize complex multi-step work
//   - Useful for long-running tasks and planning
class WriteTodoListTool : public neograph::AsyncTool {
protected:
  std::weak_ptr<agentxx::middleware::TodolistMiddlewareHandle> todolistContext;

public:
  explicit WriteTodoListTool(
      std::weak_ptr<agentxx::middleware::TodolistMiddlewareHandle>
          in_todolistContext)
      : todolistContext(in_todolistContext) {}

  std::string get_name() const override { return "todolist_write_todos"; }

  neograph::ChatTool get_definition() const override {
    return {
        "todolist_write_todos",
        R"(Create and manage a structured task list for your current work session.
Use this to plan and track complex multi-step tasks. The full todo list will be
replaced each time you call this tool. Include ALL current tasks, not just new ones.

Each todo item has:
- state: 'pending', 'in_progress', 'completed', or 'faild'
- content: description of the task
- summary: optional notes about approaches tried, lessons learned, etc.)",
        neograph::json{
            {"type", "object"},
            {
                "properties",
                {
                    {
                        "todos",
                        {
                            {"type", "array"},
                            {"description",
                             R"(The full list of todo items. Replace the entire list each call.
Include all tasks with their current status.)"},
                            {
                                "items",
                                {
                                    {"type", "object"},
                                    {
                                        "properties",
                                        {
                                            {
                                                "state",
                                                {
                                                    {"type", "string"},
                                                    {"enum",
                                                     neograph::json::array(
                                                         {"pending",
                                                          "in_progress",
                                                          "completed",
                                                          "faild"})},
                                                    {"description",
                                                     "Current status of this "
                                                     "todo item"},
                                                },
                                            },
                                            {
                                                "content",
                                                {
                                                    {"type", "string"},
                                                    {"description",
                                                     "The description of "
                                                     "the task to do"},
                                                },
                                            },
                                            {
                                                "summary",
                                                {
                                                    {"type", "string"},
                                                    {"description",
                                                     R"(Execution summary.
Record methods tried, issues encountered, optimization suggestions.
This helps when re-planning and re-executing tasks.)"},
                                                },
                                            },
                                        },
                                        {"required", neograph::json::array(
                                                         {"state", "content"})},
                                    },
                                },
                            },
                        },
                    },
                },
            },
            {"required", neograph::json::array({"todos"})},
        },
    };
  }

  asio::awaitable<std::string>
  execute_async(const neograph::json &arguments) override {
    auto thread_id = arguments.value("thread_id", std::string{});
    if (thread_id.empty()) {
      co_return R"({"error":"Toolcall inner exec faild, need `thread_id`"})";
    }

    auto todosJson = arguments.value("todos", neograph::json::array());
    if (todosJson.empty()) {
      co_return R"({"error":"Arg `todos` is empty"})";
    }

    auto handlePtr = todolistContext.lock();
    if (nullptr == handlePtr) {
      co_return R"({"error":"todolistContext is null"})";
    }

    auto state = co_await handlePtr->getStateItem(thread_id);
    state->todoStore["thread_id"] = todosJson;

    co_return "success";
  }
};
} // namespace tools
} // namespace agentxx