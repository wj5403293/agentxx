#pragma once

#include "asio/io_context.hpp"
#include "fmt/format.h"
#include "middlewares/middleware.h"
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

class PermissionMiddlewareState_c : public BaseMiddlewareState_c {
public:
  std::map<std::string, neograph::json> handles{};

  PermissionMiddlewareState_c() {}
};

class PermissionMiddlewareHandle
    : public BaseMiddlewareHandle<PermissionMiddlewareState_c> {
protected:
public:
  PermissionMiddlewareHandle(
      std::weak_ptr<MiddlewareWarpHandleContext> in_handleContext)
      : BaseMiddlewareHandle<PermissionMiddlewareState_c>(
            "PermissionMiddlewareHandle", in_handleContext) {}
};
} // namespace middleware
} // namespace agentxx