#pragma once

#include "asio/io_context.hpp"
#include "middleware.h"
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
namespace nodes {

class NEOGRAPH_API MiddlewareWrapModelCallNode
    : public MiddlewareWrapHandleBaseNode<neograph::graph::LLMCallNode> {
protected:
public:
  inline static const auto defNodeType =
      std::string_view{"xx_MiddlewareWrapModelCall"};

  MiddlewareWrapModelCallNode(
      const std::string &name, const neograph::graph::NodeContext &ctx,
      std::shared_ptr<MiddlewareWarpHandleContext> in_handleContext)
      : MiddlewareWrapHandleBaseNode<neograph::graph::LLMCallNode>(
            name, ctx, in_handleContext) {}

  asio::awaitable<void> onHandleStart(const MiddlewareWarpHandle &item,
                                      neograph::graph::NodeInput &in) override {
    if (nullptr != item.onModelcallStart) {
      co_await item.onModelcallStart(in);
    }
    co_return;
  }

  asio::awaitable<void>
  onHandleEnd(const MiddlewareWarpHandle &item,
              const neograph::graph::NodeInput &in,
              neograph::graph::NodeOutput &result) override {
    if (nullptr != item.onModelcallEnd) {
      co_await item.onModelcallEnd(in, result);
    }
    co_return;
  }
};
} // namespace nodes
} // namespace agentxx