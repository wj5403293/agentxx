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

class NEOGRAPH_API AgentStartCallWrapNode
    : public WrapHandleBaseNode<agentxx::nodes::WarpBaseNodeInterface> {
protected:
public:
  inline static constexpr auto defNodeType =
      std::string_view{"xx_MiddlewareWrapAgentStartCall"};

  AgentStartCallWrapNode(
      const std::string &name,
      std::weak_ptr<agentxx::middleware::MiddlewareWarpHandleContext>
          in_handleContext)
      : WrapHandleBaseNode<agentxx::nodes::WarpBaseNodeInterface>(
            name, in_handleContext) {}

  asio::awaitable<void>
  onHandleStart(agentxx::middleware::BaseMiddlewareHandleInterface &item,
                neograph::graph::NodeInput &in) override {
    {
      // 创建单次执行的临时数据
      auto ptr = handleContext.lock();
      ptr->graphData[in.ctx.thread_id] = std::map<std::string, std::any>{};
    }

    co_await item.onAgentcallStartFunc(in);
  }

  asio::awaitable<void>
  onHandleEnd(agentxx::middleware::BaseMiddlewareHandleInterface &item,
              const neograph::graph::NodeInput &in,
              neograph::graph::NodeOutput &result) override {
    co_return;
  }
};

class NEOGRAPH_API MiddlewareWrapAgentEndCallNode
    : public WrapHandleBaseNode<agentxx::nodes::WarpBaseNodeInterface> {
protected:
public:
  inline static constexpr auto defNodeType =
      std::string_view{"xx_MiddlewareWrapAgentEndCall"};

  MiddlewareWrapAgentEndCallNode(
      const std::string &name,
      std::weak_ptr<agentxx::middleware::MiddlewareWarpHandleContext>
          in_handleContext)
      : WrapHandleBaseNode<agentxx::nodes::WarpBaseNodeInterface>(
            name, in_handleContext) {}

  asio::awaitable<void>
  onHandleStart(agentxx::middleware::BaseMiddlewareHandleInterface &item,
                neograph::graph::NodeInput &in) override {
    co_return;
  }

  asio::awaitable<void>
  onHandleEnd(agentxx::middleware::BaseMiddlewareHandleInterface &item,
              const neograph::graph::NodeInput &in,
              neograph::graph::NodeOutput &result) override {
    co_await item.onAgentcallEndFunc(in, result);

    {
      // 清理单次执行的临时数据
      auto ptr = handleContext.lock();
      auto it = ptr->graphData.find(in.ctx.thread_id);
      if (it != ptr->graphData.end()) {
        ptr->graphData.erase(it);
      }
    }
  }
};

} // namespace nodes
} // namespace agentxx