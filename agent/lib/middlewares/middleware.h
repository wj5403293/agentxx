#pragma once

#include "asio/io_context.hpp"
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <neograph/llm/rate_limited_provider.h>
#include <neograph/llm/schema_provider.h>
#include <neograph/neograph.h>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace agentxx {
namespace middleware {

typedef std::function<asio::awaitable<void>(neograph::graph::NodeInput &in)>
    onGraphNodeBeforeCallFunc;
typedef std::function<asio::awaitable<void>(
    const neograph::graph::NodeInput &in, neograph::graph::NodeOutput &result)>
    onGraphNodeAfterCallFunc;

class MiddlewareWarpHandleBase {
public:
  std::string name;
  /// 会被添加移动到 agent 中，完成后此处留空数组
  std::vector<std::unique_ptr<neograph::Tool>> toolcalls{};
  /// 谨慎存储/修改 middleware 中的变量，
  /// 这是一个agent中所有会话共享的，单会话变量应该放 state 内

  MiddlewareWarpHandleBase(const std::string &in_name) : name(in_name) {}

  virtual asio::awaitable<void>
  onModelcallStartFunc(neograph::graph::NodeInput &in) = 0;

  virtual asio::awaitable<void>
  onModelcallEndFunc(const neograph::graph::NodeInput &in,
                     neograph::graph::NodeOutput &result) = 0;

  virtual asio::awaitable<void>
  onToolcallStartFunc(neograph::graph::NodeInput &in) = 0;

  virtual asio::awaitable<void>
  onToolcallEndFunc(const neograph::graph::NodeInput &in,
                    neograph::graph::NodeOutput &result) = 0;
};

class MiddlewareWarpHandle : public MiddlewareWarpHandleBase {
public:
  onGraphNodeBeforeCallFunc onModelcallStart;
  onGraphNodeAfterCallFunc onModelcallEnd;
  onGraphNodeBeforeCallFunc onToolcallStart;
  onGraphNodeAfterCallFunc onToolcallEnd;

  MiddlewareWarpHandle(
      const std::string &in_name,
      const onGraphNodeBeforeCallFunc &in_onModelcallStart = nullptr,
      const onGraphNodeAfterCallFunc &in_onModelcallEnd = nullptr,
      const onGraphNodeBeforeCallFunc &in_onToolcallStart = nullptr,
      const onGraphNodeAfterCallFunc &in_onToolcallEnd = nullptr)
      : MiddlewareWarpHandleBase(in_name),
        onModelcallStart(in_onModelcallStart),
        onModelcallEnd(in_onModelcallEnd), onToolcallStart(in_onToolcallStart),
        onToolcallEnd(in_onToolcallEnd) {}

  virtual asio::awaitable<void>
  onModelcallStartFunc(neograph::graph::NodeInput &in) {
    if (nullptr != onModelcallStart) {
      co_await onModelcallStart(in);
    }
  }

  virtual asio::awaitable<void>
  onModelcallEndFunc(const neograph::graph::NodeInput &in,
                     neograph::graph::NodeOutput &result) {
    if (nullptr != onModelcallEnd) {
      co_await onModelcallEnd(in, result);
    }
  }

  virtual asio::awaitable<void>
  onToolcallStartFunc(neograph::graph::NodeInput &in) {
    if (nullptr != onToolcallStart) {
      co_await onToolcallStart(in);
    }
  }

  virtual asio::awaitable<void>
  onToolcallEndFunc(const neograph::graph::NodeInput &in,
                    neograph::graph::NodeOutput &result) {
    if (nullptr != onToolcallEnd) {
      co_await onToolcallEnd(in, result);
    }
  }
};

class MiddlewareWarpHandleContext {
public:
  std::vector<std::unique_ptr<MiddlewareWarpHandleBase>> handles{};

  MiddlewareWarpHandleContext() {}
};

} // namespace middleware
} // namespace agentxx