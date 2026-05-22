#pragma once

#include "asio/io_context.hpp"
#include <cstdlib>
#include <functional>
#include <iostream>
#include <neograph/graph/deep_research_graph.h>
#include <neograph/llm/rate_limited_provider.h>
#include <neograph/llm/schema_provider.h>
#include <neograph/neograph.h>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace agentxx {
namespace nodes {

class NEOGRAPH_API ToolcallNode : public neograph::graph::ToolDispatchNode {
protected:
  std::string name;
  std::function<void(neograph::graph::NodeInput &in)> onToolcallStart;
  std::function<void(const neograph::graph::NodeInput &in,
                     neograph::graph::NodeOutput &result)>
      onToolcallEnd;

public:
  inline static const auto defNodeType = std::string_view{"xx_tool_call"};

  ToolcallNode(const std::string &in_name,
               const neograph::graph::NodeContext &ctx,
               const std::function<void(neograph::graph::NodeInput &in)>
                   &in_onToolcallStart = nullptr,
               const std::function<void(const neograph::graph::NodeInput &in,
                                        neograph::graph::NodeOutput &result)>
                   &in_onToolcallEnd = nullptr)
      : neograph::graph::ToolDispatchNode(in_name, ctx), name(in_name),
        onToolcallStart(in_onToolcallStart), onToolcallEnd(in_onToolcallEnd) {}

  asio::awaitable<neograph::graph::NodeOutput>
  run(neograph::graph::NodeInput in) override {
    try {
      if (nullptr != onToolcallStart) {
        onToolcallStart(in);
      }
      auto result = co_await neograph::graph::ToolDispatchNode::run(in);
      if (nullptr != onToolcallEnd) {
        onToolcallEnd(in, result);
      }
      co_return result;
    } catch (const std::exception &e) {
      neograph::graph::NodeOutput out;
      out.writes.push_back(neograph::graph::ChannelWrite{
          "messages",
          std::format(R"({{"error": "Tool `{}` exception: {}"}})", name,
                      e.what()),
      });
      if (nullptr != onToolcallEnd) {
        onToolcallEnd(in, out);
      }
      co_return out;
    }
  }

  inline static void defStdoutLogOnToolcallStart(neograph::graph::NodeInput &in,
                                                 const std::string_view name,
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
┏━━━━━━ Toolcall/{} ━━━━━━┓
{}
)",
                             name, out.str())
              << std::flush;
  }

  inline static void defStdoutLogOnToolcallEnd(
      const neograph::graph::NodeInput &in, neograph::graph::NodeOutput &result,
      const std::string_view name, size_t limitOutput = 0) {
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
{}
┗━━━━━━  Done / {}  ━━━━━━┛
)",
                             out.str(), name)
              << std::endl;
  }
};
} // namespace nodes
} // namespace agentxx