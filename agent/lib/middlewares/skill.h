#pragma once

#include "asio/io_context.hpp"
#include "middlewares/middleware.h"
#include <cstdlib>
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

class _SkillData_c {
public:
};

class MiddlewareWarpSkillHandle : public MiddlewareWarpHandleBase {
protected:
  std::set<std::string> skillPaths{};

  /// <path, data>
  std::map<std::string, _SkillData_c> skillData{};

  /// <path, error>
  std::map<std::string, std::string> loadErrors{};

public:
  MiddlewareWarpSkillHandle() : MiddlewareWarpHandleBase("SkillManager") {}

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