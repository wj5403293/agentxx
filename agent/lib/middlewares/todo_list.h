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

class TodolistMiddlewareState_c : public BaseMiddlewareState_c {
public:
  /// <thread_id, todoListJson>
  /// [会话独立] 任务规划列表，由 todolist tool 读写
  std::map<std::string, neograph::json> todoStore{};

  TodolistMiddlewareState_c() {}
};

class TodolistMiddlewareHandle
    : public BaseMiddlewareHandle<TodolistMiddlewareState_c> {
protected:
public:
  TodolistMiddlewareHandle(
      std::weak_ptr<MiddlewareWarpHandleContext> in_handleContext)
      : BaseMiddlewareHandle<TodolistMiddlewareState_c>(
            "TodolistMiddlewareHandle", in_handleContext) {}

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