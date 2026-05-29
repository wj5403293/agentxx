#pragma once

#include "asio/io_context.hpp"
#include "fmt/format.h"
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
namespace nodes {

template <typename T>
concept BaseGraphNodeType = std::same_as<T, neograph::graph::GraphNode> ||
                            std::derived_from<T, neograph::graph::GraphNode>;

template <BaseGraphNodeType T>
class NEOGRAPH_API MiddlewareWrapBaseNode : public T {

protected:
  std::string name;
  agentxx::middleware::onGraphNodeBeforeCallFunc onBeforeCall;
  agentxx::middleware::onGraphNodeAfterCallFunc onAfterCall;

public:
  MiddlewareWrapBaseNode(
      const std::string &in_name, const neograph::graph::NodeContext &ctx,
      const agentxx::middleware::onGraphNodeBeforeCallFunc &in_onBeforeCall =
          nullptr,
      const agentxx::middleware::onGraphNodeAfterCallFunc &in_onAfterCall =
          nullptr)
      : name(in_name), onBeforeCall(in_onBeforeCall),
        onAfterCall(in_onAfterCall) {}

  virtual asio::awaitable<void>
  onBeforeCallFunc(neograph::graph::NodeInput &in) {
    if (nullptr != onBeforeCall) {
      co_return onBeforeCall(in);
    }
  }

  virtual asio::awaitable<void>
  onAfterCallFunc(const neograph::graph::NodeInput &in,
                  neograph::graph::NodeOutput &result) {
    if (nullptr != onAfterCall) {
      co_return onAfterCall(in, result);
    }
  }

  virtual asio::awaitable<neograph::graph::NodeOutput>
  baseRun(neograph::graph::NodeInput &in) {
    try {
      co_return co_await neograph::graph::LLMCallNode::run(in);
    } catch (const std::exception &e) {
      neograph::graph::NodeOutput out;
      out.writes.push_back(neograph::graph::ChannelWrite{
          "messages",
          fmt::format(R"({{"error": "Middleware Wrap `{}` exception: {}"}})",
                      name, e.what()),
      });
      co_return out;
    }
  }

  asio::awaitable<neograph::graph::NodeOutput>
  run(neograph::graph::NodeInput in) override {
    co_await onBeforeCallFunc(in);
    auto result = co_await T::run(in);
    co_await onAfterCallFunc(in, result);
    co_return result;
  }
};

template <BaseGraphNodeType T>
class NEOGRAPH_API MiddlewareWrapHandleBaseNode : public T {
protected:
  std::string nodeName;
  std::shared_ptr<agentxx::middleware::MiddlewareWarpHandleContext>
      handleContext;

public:
  MiddlewareWrapHandleBaseNode(
      const std::string &name, const neograph::graph::NodeContext &ctx,
      std::shared_ptr<agentxx::middleware::MiddlewareWarpHandleContext>
          in_handleContext)
      : T(name, ctx), nodeName(name), handleContext(in_handleContext) {}

  virtual asio::awaitable<neograph::graph::NodeOutput>
  baseRun(neograph::graph::NodeInput &in) {
    try {
      co_return co_await T::run(in);
    } catch (const std::exception &e) {
      neograph::graph::NodeOutput out;
      out.writes.push_back(neograph::graph::ChannelWrite{
          "messages",
          fmt::format(R"({{"error": "{}/LLMCall exception: {}"}})", nodeName,
                      e.what()),
      });
      co_return out;
    }
  }

  virtual asio::awaitable<void>
  onHandleStart(agentxx::middleware::MiddlewareWarpHandleBase &item,
                neograph::graph::NodeInput &in) = 0;

  virtual asio::awaitable<void>
  onHandleEnd(agentxx::middleware::MiddlewareWarpHandleBase &item,
              const neograph::graph::NodeInput &in,
              neograph::graph::NodeOutput &result) = 0;

  asio::awaitable<neograph::graph::NodeOutput>
  run(neograph::graph::NodeInput in) override {
    neograph::graph::NodeOutput out;

    size_t i = 0;
    const auto len = handleContext->handles.size();
    for (; i < len; ++i) {
      auto &item = handleContext->handles[i];
      try {
        co_await onHandleStart(*item, in);
      } catch (const std::exception &e) {
        out.writes.push_back(neograph::graph::ChannelWrite{
            "messages",
            fmt::format(R"({{"error": "{}/Start call `{}` exception: {}"}})",
                        nodeName, item->name, e.what()),
        });
        break;
      }
    }

    if (i >= len) {
      out = co_await baseRun(in);
      i = len;
    }

    for (; i-- > 0;) {
      auto &item = handleContext->handles[i];
      try {
        co_await onHandleEnd(*item, in, out);
      } catch (const std::exception &e) {
        out.writes.push_back(neograph::graph::ChannelWrite{
            "messages",
            fmt::format(R"({{"error": "{}/End call `{}` exception: {}"}})",
                        nodeName, item->name, e.what()),
        });
      }
    }
    co_return out;
  }
};

} // namespace nodes
} // namespace agentxx