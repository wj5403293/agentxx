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

namespace asio = ::boost::asio;

namespace agentxx {
namespace nodes {

template <typename T>
concept BaseGraphNodeType = std::same_as<T, neograph::graph::GraphNode> ||
                            std::derived_from<T, neograph::graph::GraphNode>;

class WarpBaseNodeInterface : public neograph::graph::GraphNode {
protected:
  std::string name_;

public:
  WarpBaseNodeInterface(const std::string &name) : name_(name) {}

  asio::awaitable<neograph::graph::NodeOutput>
  run(neograph::graph::NodeInput in) override {
    co_return neograph::graph::NodeOutput{};
  }

  std::string get_name() const override { return name_; }
};

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
      co_return co_await T::run(in);
    } catch (const neograph::graph::NodeInterrupt &e) {
      throw e;
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
  run(neograph::graph::NodeInput in) override final {
    co_await onBeforeCallFunc(in);
    auto result = co_await baseRun(in);
    co_await onAfterCallFunc(in, result);
    co_return result;
  }
};

template <BaseGraphNodeType T>
class NEOGRAPH_API WrapHandleBaseNode : public T {
protected:
  std::string nodeName;
  std::weak_ptr<agentxx::middleware::MiddlewareWarpHandleContext> handleContext;

public:
  template <typename... Args>
  WrapHandleBaseNode(
      const std::string &name,
      std::weak_ptr<agentxx::middleware::MiddlewareWarpHandleContext>
          in_handleContext,
      Args &&...args)
      : T(name, std::forward<Args>(args)...), nodeName(name),
        handleContext(in_handleContext) {}

  virtual asio::awaitable<void>
  onHandleStart(agentxx::middleware::BaseMiddlewareHandleInterface &item,
                neograph::graph::NodeInput &in) = 0;

  virtual asio::awaitable<void>
  onHandleEnd(agentxx::middleware::BaseMiddlewareHandleInterface &item,
              const neograph::graph::NodeInput &in,
              neograph::graph::NodeOutput &result) = 0;

  // 如果是消息节点，应当添加消息，后续不执行 BaseRun
  virtual void
  onHandleStartError(bool errorRethrow, const std::exception *e,
                     agentxx::middleware::BaseMiddlewareHandleInterface &item,
                     neograph::graph::NodeInput &in,
                     neograph::graph::NodeOutput &result) {}
  virtual void onHandleBaseRunError(bool errorRethrow, const std::exception *e,
                                    neograph::graph::NodeInput &in,
                                    neograph::graph::NodeOutput &result) {}
  // 一般不修改 [result]，消息已经由前面的 start/baseRun 添加
  virtual void
  onHandleEndError(bool errorRethrow, const std::exception *e,
                   agentxx::middleware::BaseMiddlewareHandleInterface &item,
                   const neograph::graph::NodeInput &in,
                   neograph::graph::NodeOutput &result) {}

  virtual asio::awaitable<void> baseRun(neograph::graph::NodeInput &in,
                                        neograph::graph::NodeOutput &result) {
    result = co_await T::run(in);
  }

  /// 栈式调用和异常处理
  /// start1 -> start2 -> start3
  ///             v         |
  ///           error     baseRun
  ///             v         |
  ///  end1  <-  end2  <-  end3
  ///
  /// - start 出现错误时，跳过 baseRun，执行对应的 end
  asio::awaitable<neograph::graph::NodeOutput>
  run(neograph::graph::NodeInput in) override final {
    std::exception_ptr errorPtr;
    bool errorRethrow = false;
    neograph::graph::NodeOutput out;

    auto handleContextPtr = handleContext.lock();
    const auto len = handleContextPtr->handles.size();
    size_t i = 0;
    for (; i < len; ++i) {
      auto &item = handleContextPtr->handles[i];
      try {
        co_await onHandleStart(*item, in);
      } catch (const neograph::graph::NodeInterrupt &e) {
        errorRethrow = true;
        onHandleStartError(errorRethrow, &e, *item, in, out);
        errorPtr = std::current_exception();
        break;
      } catch (const std::exception &e) {
        XX_LOGE("{}/Start call `{}` exception: {}", nodeName, item->name,
                e.what());
        // 替代 baseRun
        onHandleStartError(errorRethrow, &e, *item, in, out);
        errorPtr = std::current_exception();
        break;
      }
    }

    if (i >= len) {
      try {
        co_await baseRun(in, out);
      } catch (const neograph::graph::NodeInterrupt &e) {
        errorRethrow = true;
        onHandleBaseRunError(errorRethrow, &e, in, out);
        errorPtr = std::current_exception();
      } catch (const std::exception &e) {
        XX_LOGE("{}/run exception: {})", nodeName, e.what());
        onHandleBaseRunError(errorRethrow, &e, in, out);
        errorPtr = std::current_exception();
      }
      i = len;
    } else if (nullptr != errorPtr) {
      onHandleBaseRunError(errorRethrow, nullptr, in, out);
    }

    for (; i-- > 0;) {
      auto &item = handleContextPtr->handles[i];
      if (nullptr != errorPtr) {
        onHandleEndError(errorRethrow, nullptr, *item, in, out);
      } else {
        try {
          co_await onHandleEnd(*item, in, out);
        } catch (const neograph::graph::NodeInterrupt &e) {
          errorRethrow = true;
          onHandleEndError(errorRethrow, &e, *item, in, out);
          errorPtr = std::current_exception();
        } catch (const std::exception &e) {
          XX_LOGE("{}/End call `{}` exception: {}", nodeName, item->name,
                  e.what());
          onHandleEndError(errorRethrow, &e, *item, in, out);
          errorPtr = std::current_exception();
        }
      }
    }

    if (errorRethrow) {
      std::rethrow_exception(errorPtr);
    }

    co_return out;
  }
};

} // namespace nodes
} // namespace agentxx