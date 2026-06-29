#pragma once

#include "agentxx/middlewares/middleware.h"
#include "asio/io_context.hpp"
#include "fmt/format.h"
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <neograph/neograph.h>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace agentxx {
namespace middleware {

class PermissionMiddlewareState : public BaseMiddlewareState {
public:
  PermissionMiddlewareState() {}
};

class PermissionMiddlewareHandle
    : public BaseMiddlewareHandle<PermissionMiddlewareState> {
protected:
public:
  /// <name, handle>
  std::map<std::string,
           std::function<asio::awaitable<bool>(const neograph::Tool &item)>>
      handles{};

  PermissionMiddlewareHandle(
      std::weak_ptr<agentxx::agent::AgentContext> in_agentContext)
      : BaseMiddlewareHandle<PermissionMiddlewareState>(
            "PermissionMiddlewareHandle", in_agentContext) {}

  void registerHandles() {
    handles["filesystem_list_file"] =
        [](const neograph::Tool &item) -> asio::awaitable<bool> {
      co_return true;
    };
  }
};

} // namespace middleware
} // namespace agentxx