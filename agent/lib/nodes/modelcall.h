#pragma once

#include "asio/io_context.hpp"
#include "nodes/middleware_handle.h"
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
  inline static constexpr auto defNodeType =
      std::string_view{"xx_MiddlewareWrapModelCall"};

  MiddlewareWrapModelCallNode(
      const std::string &name, const neograph::graph::NodeContext &ctx,
      std::shared_ptr<agentxx::middleware::MiddlewareWarpHandleContext>
          in_handleContext)
      : MiddlewareWrapHandleBaseNode<neograph::graph::LLMCallNode>(
            name, ctx, in_handleContext) {}

  asio::awaitable<void>
  onHandleStart(agentxx::middleware::BaseMiddlewareHandleInterface &item,
                neograph::graph::NodeInput &in) override {
    co_await item.onModelcallStartFunc(in);
  }

  asio::awaitable<void>
  onHandleEnd(agentxx::middleware::BaseMiddlewareHandleInterface &item,
              const neograph::graph::NodeInput &in,
              neograph::graph::NodeOutput &result) override {
    co_await item.onModelcallEndFunc(in, result);
  }
};
} // namespace nodes
} // namespace agentxx