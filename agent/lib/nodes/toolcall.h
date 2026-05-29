#pragma once

#include "asio/io_context.hpp"
#include "fmt/base.h"
#include "fmt/format.h"
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

class NEOGRAPH_API MiddlewareWrapToolcallNode
    : public MiddlewareWrapHandleBaseNode<neograph::graph::ToolDispatchNode> {
protected:
public:
  inline static const auto defNodeType = std::string_view{"xx_Toolcall"};

  MiddlewareWrapToolcallNode(
      const std::string &in_name, const neograph::graph::NodeContext &in_ctx,
      std::shared_ptr<agentxx::middleware::MiddlewareWarpHandleContext>
          in_handleContext)
      : MiddlewareWrapHandleBaseNode<neograph::graph::ToolDispatchNode>(
            in_name, in_ctx, in_handleContext) {}

  asio::awaitable<void>
  onHandleStart(agentxx::middleware::MiddlewareWarpHandleBase &item,
                neograph::graph::NodeInput &in) override {
    co_await item.onToolcallStartFunc(in);
  }

  asio::awaitable<void>
  onHandleEnd(agentxx::middleware::MiddlewareWarpHandleBase &item,
              const neograph::graph::NodeInput &in,
              neograph::graph::NodeOutput &result) override {
    co_await item.onToolcallEndFunc(in, result);
  }

  inline static asio::awaitable<void>
  defStdoutLogOnToolcallStart(neograph::graph::NodeInput &in,
                              size_t limitOutput = 0) {
    const neograph::ChatMessage *assistant_msg = nullptr;
    auto messages = in.state.get_messages();
    if (false == messages.empty()) {
      // Find the last assistant message with tool_calls
      for (auto it = messages.rbegin(); it != messages.rend(); ++it) {
        if (it->role == "assistant" && !it->tool_calls.empty()) {
          assistant_msg = &(*it);
          break;
        }
      }
    }

    std::ostringstream out{};
    if (assistant_msg) {
      size_t index = 0;
      for (auto &item : assistant_msg->tool_calls) {
        ++index;
        out << "┣━ Argument: " << index << ". " << item.name << "/" << item.id
            << std::endl;
        out << "             - "
            << ((0 == limitOutput)
                    ? item.arguments
                    : std::string_view{item.arguments}.substr(0, limitOutput))
            << std::endl;
      }
    } else {
      out << "┣━ Empty Argument List\n";
    }

    fmt::print(R"(
┏━━━━━━ Toolcall  Run ━━━━━━┓
{}
)",
               out.str());
    co_return;
  }

  inline static asio::awaitable<void>
  defStdoutLogOnToolcallEnd(const neograph::graph::NodeInput &in,
                            neograph::graph::NodeOutput &result,
                            size_t limitOutput = 0) {
    std::ostringstream out{};
    if (false == result.writes.empty()) {
      size_t index = 0;
      for (auto &item : result.writes) {
        ++index;
        auto str = item.value.dump();
        out << "┣━ Result  : " << index << ". " << item.channel << ": "
            << ((0 == limitOutput)
                    ? str
                    : std::string_view{str}.substr(0, limitOutput))
            << std::endl;
      }
    } else {
      out << "┣━ Empty Result List\n";
    }

    fmt::print(R"(
{}
┗━━━━━━ Toolcall Done ━━━━━━┛

)",
               out.str());
    co_return;
  }
};
} // namespace nodes
} // namespace agentxx