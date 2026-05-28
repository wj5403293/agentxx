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

class NEOGRAPH_API MiddlewareWrapToolcallNode
    : public MiddlewareWrapHandleBaseNode<neograph::graph::ToolDispatchNode> {
protected:
public:
  inline static const auto defNodeType = std::string_view{"xx_Toolcall"};

  MiddlewareWrapToolcallNode(
      const std::string &in_name, const neograph::graph::NodeContext &in_ctx,
      std::shared_ptr<MiddlewareWarpHandleContext> in_handleContext)
      : MiddlewareWrapHandleBaseNode<neograph::graph::ToolDispatchNode>(
            in_name, in_ctx, in_handleContext) {}

  asio::awaitable<void> onHandleStart(const MiddlewareWarpHandle &item,
                                      neograph::graph::NodeInput &in) override {
    if (nullptr != item.onToolcallStart) {
      co_await item.onToolcallStart(in);
    }
    co_return;
  }

  asio::awaitable<void>
  onHandleEnd(const MiddlewareWarpHandle &item,
              const neograph::graph::NodeInput &in,
              neograph::graph::NodeOutput &result) override {
    if (nullptr != item.onToolcallEnd) {
      co_await item.onToolcallEnd(in, result);
    }
    co_return;
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

    std::cout << std::format(R"(
┏━━━━━━ Toolcall  Run ━━━━━━┓
{}
╿
)",
                             out.str())
              << std::flush;
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

    std::cout << std::format(R"(
╽
{}
┗━━━━━━ Toolcall Done ━━━━━━┛
)",
                             out.str())
              << std::endl;
    co_return;
  }
};
} // namespace nodes
} // namespace agentxx