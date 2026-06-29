#pragma once

#include "agentxx/nodes/warp_handle.h"
#include "agentxx/util/exception.h"
#include "agentxx/util/log.h"
#include "asio/io_context.hpp"
#include "asio/steady_timer.hpp"
#include "asio/use_awaitable.hpp"
#include <cstdlib>
#include <functional>
#include <iostream>
#include <neograph/llm/rate_limited_provider.h>
#include <neograph/llm/schema_provider.h>
#include <neograph/neograph.h>
#include <neograph/types.h>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace agentxx {
namespace nodes {

class NEOGRAPH_API ModelCallWrapNode
    : public WrapHandleBaseNode<neograph::graph::LLMCallNode> {
protected:
public:
  inline static constexpr auto defNodeType =
      std::string_view{"xx_MiddlewareWrapModelCall"};

  ModelCallWrapNode(const std::string &name,
                    const neograph::graph::NodeContext &ctx,
                    std::weak_ptr<agentxx::agent::AgentContext> in_agentContext)
      : WrapHandleBaseNode<neograph::graph::LLMCallNode>(name, in_agentContext,
                                                         ctx) {}

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

  void
  onHandleStartError(bool errorRethrow, bool isCurrentError,
                     std::string_view exceptionStr,
                     agentxx::middleware::BaseMiddlewareHandleInterface &item,
                     neograph::graph::NodeInput &in,
                     neograph::graph::NodeOutput &result) noexcept override {
    // 插入消息，保证消息顺序正确
    if (false == errorRethrow) {
      auto msg = neograph::ChatMessage{
          .role = "assistant",
          .content =
              fmt::format(R"({{"error": "{}/Start call `{}` exception: {}"}})",
                          nodeName, item.name, exceptionStr)};
      auto msgJson = neograph::json{};
      neograph::to_json(msgJson, msg);
      result.writes.push_back(neograph::graph::ChannelWrite{
          "messages",
          msgJson,
      });
    }
  }

  void
  onHandleBaseRunError(bool errorRethrow, bool isCurrentError,
                       std::string_view exceptionStr,
                       neograph::graph::NodeInput &in,
                       neograph::graph::NodeOutput &result) noexcept override {
    // 插入消息，保证消息顺序正确
    if (false == errorRethrow && isCurrentError) {
      auto msg = neograph::ChatMessage{
          .role = "assistant",
          .content = fmt::format(R"({{"error": "{}/run exception: {}"}})",
                                 nodeName, exceptionStr)};
      auto msgJson = neograph::json{};
      neograph::to_json(msgJson, msg);
      result.writes.push_back(neograph::graph::ChannelWrite{
          "messages",
          msgJson,
      });
    }
  }

  asio::awaitable<void> baseRun(neograph::graph::NodeInput &in,
                                neograph::graph::NodeOutput &result) override {
    auto agentCtxPtr = agentContext.lock();

    // 添加 system Msg
    auto msglist = in.state.get("messages");
    bool haveSystemMsg = false;
    auto newSystemMsg = neograph::ChatMessage{.role = "system"};
    if (msglist.is_array() && false == msglist.empty()) {
      auto systemMsg = neograph::ChatMessage{};
      from_json(msglist.front(), systemMsg);
      if (systemMsg.role == "system") {
        haveSystemMsg = true;
        newSystemMsg = std::move(systemMsg);
      }
    }

    {
      auto &appendSystemMsgList =
          agentCtxPtr->middlewareHandleContext
              ->getGraphDataItemValue<std::vector<std::string>>(
                  in.ctx.thread_id, agentxx::middleware::MiddlewareContext::
                                        graphDataKey_systemMessage);

      // 清空原本的 content
      newSystemMsg.content = "";
      std::ostringstream oss;
      oss << agentCtxPtr->agentConfig->prompt.systemPrompt;

      if (false == appendSystemMsgList.empty()) {
        for (const auto &item : appendSystemMsgList) {
          oss << item << "\n";
        }
      }

      newSystemMsg.content = oss.str();
    }

    neograph::json sysMsgJson;
    neograph::to_json(sysMsgJson, newSystemMsg);
    if (haveSystemMsg) {
      // 替换 system msg
      msglist[0] = std::move(sysMsgJson);
    } else {
      // 缺少 system msg，在开头插入
      auto newlist = neograph::json::array();
      newlist.push_back(std::move(sysMsgJson));
      for (auto item : msglist.items()) {
        newlist.push_back(std::move(item.second));
      }
      msglist = std::move(newlist);
    }
    in.state.overwrite("messages", msglist);

    if (agentCtxPtr->agentConfig->logPrintMessagesBeforeLLM) {
      agentxx::middleware::BaseMiddlewareHandleInterface::printMessages(
          in.state.get_messages());
    }

    size_t retry = 0;
    auto timer = asio::steady_timer(co_await asio::this_coro::executor);
    do {
      std::string errInfo;
      try {
        co_await WrapHandleBaseNode<neograph::graph::LLMCallNode>::baseRun(
            in, result);
        co_return;
      } catch (const neograph::graph::CancelledException &e) {
        throw;
        // } catch (const neograph::graph::NodeInterrupt &e) {
        // llm node 无 Interrupt
      } catch (const std::exception &e) {
        if (retry >= agentCtxPtr->agentConfig->llmMaxRetry) {
          throw;
        }
        errInfo = e.what();
      } catch (const boost::exception &e) {
        if (retry >= agentCtxPtr->agentConfig->llmMaxRetry) {
          throw;
        }
        errInfo = boost::diagnostic_information(e);
      } catch (...) {
        if (retry >= agentCtxPtr->agentConfig->llmMaxRetry) {
          throw;
        }
      }

      // 自动重试
      // TODO: 附加已有的 llm 消息，而不是覆盖
      retry++;
      XX_LOGD("LLMCallNode retry: {}/{} | {}", retry,
              agentCtxPtr->agentConfig->llmMaxRetry, errInfo);
      // 延时等待
      timer.expires_after(std::chrono::milliseconds(retry * 1000));
      co_await timer.async_wait(asio::use_awaitable);
    } while (true);
  }
};
} // namespace nodes
} // namespace agentxx