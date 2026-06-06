#pragma once

#include "asio/io_context.hpp"
#include <any>
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

class BaseMiddlewareState_c {
public:
  BaseMiddlewareState_c() {}

  virtual ~BaseMiddlewareState_c() {}
};

template <typename T>
concept BaseMiddlewareStateType = std::same_as<T, BaseMiddlewareState_c> ||
                                  std::derived_from<T, BaseMiddlewareState_c>;

class MiddlewareWarpHandleContext;

/// 接口类型
/// - 主要用于接收多种泛型参数, 见
/// [MiddlewareWarpHandleContext::handles]，handles
///   需要接收多种不同继承后的模版类型
///   BaseMiddlewareHandle<BaseMiddlewareStateType>，当 state
///   被继承时编译会失败，因此拉出 [BaseMiddlewareHandleInterface] 无 state
///   模版参数作为基本类型
class BaseMiddlewareHandleInterface {
protected:
public:
  /// 谨慎存储/修改 middleware 中的变量，
  /// 这是一个agent中所有会话共享的，单会话变量应该放 state 内

  /// 名称
  std::string name;
  /// 会被添加移动到 agent 中，完成后此处留空数组
  std::vector<std::unique_ptr<neograph::Tool>> toolcalls{};
  /// 每个 [Middleware] 全局共享，按会话ID 取值 <thread_id, state>
  std::map<std::string, std::shared_ptr<BaseMiddlewareState_c>> states{};
  std::weak_ptr<MiddlewareWarpHandleContext> handleContext;

  BaseMiddlewareHandleInterface(
      const std::string &in_name,
      std::weak_ptr<MiddlewareWarpHandleContext> in_handleContext)
      : name(in_name), handleContext(in_handleContext) {}

  /// ================ warp call ================
  virtual asio::awaitable<void>
  onAgentcallStartFunc(neograph::graph::NodeInput &in) = 0;

  virtual asio::awaitable<void>
  onAgentcallEndFunc(const neograph::graph::NodeInput &in,
                     neograph::graph::NodeOutput &result) = 0;

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

  virtual ~BaseMiddlewareHandleInterface() = default;
};

template <BaseMiddlewareStateType T>
class BaseMiddlewareHandle : public BaseMiddlewareHandleInterface {
protected:
public:
  BaseMiddlewareHandle(
      const std::string &in_name,
      std::weak_ptr<MiddlewareWarpHandleContext> in_handleContext)
      : BaseMiddlewareHandleInterface(in_name, in_handleContext) {}

  /// ================ state ================
  virtual asio::awaitable<void>
  stateReadBlock(const std::function<asio::awaitable<void>()> &func) {
    if (nullptr != func) {
      co_await func();
    }
  }

  virtual asio::awaitable<void>
  stateWriteBlock(const std::function<asio::awaitable<void>()> &func) {
    if (nullptr != func) {
      co_await func();
    }
  }

  /// 延迟加载 state
  /// - 如果 thread 很多，可以等需要时从硬盘加载进内存
  virtual asio::awaitable<std::shared_ptr<T>>
  loadStateItem(const std::string &thread_id) {
    // TODO: 从磁盘读取
    auto ptr = std::make_shared<T>();
    states[thread_id] = ptr;
    co_return ptr;
  }

  virtual asio::awaitable<std::shared_ptr<T>>
  getStateItem(const std::string &thread_id) {
    {
      auto it = states.find(thread_id);
      if (it != states.end()) {
        co_return (std::static_pointer_cast<T>(it->second));
      }
    }
    co_return co_await loadStateItem(thread_id);
  }

  virtual asio::awaitable<void> saveStateItem(const std::string &thread_id,
                                              bool offload = true) {
    std::shared_ptr<agentxx::middleware::BaseMiddlewareState_c> oldEntity =
        nullptr;
    bool doSave = false;
    if (offload) {
      {
        auto it = states.find(thread_id);
        if (it != states.end()) {
          doSave = true;
          oldEntity = states.erase(it)->second;
        }
      }
    } else {
      auto it = states.find(thread_id);
      if (it != states.end()) {
        doSave = true;
        oldEntity = it->second;
      }
    }
    if (doSave && nullptr != oldEntity) {
      // TODO: old 写入磁盘
    }
    co_return;
  }

  virtual bool containsItem(const std::string &thread_id) {
    return states.contains(thread_id);
  }
};

template <BaseMiddlewareStateType T>
class MiddlewareWarpHandle : public BaseMiddlewareHandle<T> {
public:
  onGraphNodeBeforeCallFunc onAgentcallStart;
  onGraphNodeAfterCallFunc onAgentcallEnd;
  onGraphNodeBeforeCallFunc onModelcallStart;
  onGraphNodeAfterCallFunc onModelcallEnd;
  onGraphNodeBeforeCallFunc onToolcallStart;
  onGraphNodeAfterCallFunc onToolcallEnd;

  MiddlewareWarpHandle(
      const std::string &in_name,
      std::weak_ptr<MiddlewareWarpHandleContext> in_handleContext,
      const onGraphNodeBeforeCallFunc &in_onAgentcallStart = nullptr,
      const onGraphNodeAfterCallFunc &in_onAgentcallEnd = nullptr,
      const onGraphNodeBeforeCallFunc &in_onModelcallStart = nullptr,
      const onGraphNodeAfterCallFunc &in_onModelcallEnd = nullptr,
      const onGraphNodeBeforeCallFunc &in_onToolcallStart = nullptr,
      const onGraphNodeAfterCallFunc &in_onToolcallEnd = nullptr)
      : BaseMiddlewareHandle<T>(in_name, in_handleContext),
        onAgentcallStart(in_onAgentcallStart),
        onAgentcallEnd(in_onAgentcallEnd),
        onModelcallStart(in_onModelcallStart),
        onModelcallEnd(in_onModelcallEnd), onToolcallStart(in_onToolcallStart),
        onToolcallEnd(in_onToolcallEnd) {}

  asio::awaitable<void>
  onAgentcallStartFunc(neograph::graph::NodeInput &in) override {
    if (nullptr != onAgentcallStart) {
      co_await onAgentcallStart(in);
    }
  }

  asio::awaitable<void>
  onAgentcallEndFunc(const neograph::graph::NodeInput &in,
                     neograph::graph::NodeOutput &result) override {
    if (nullptr != onAgentcallEnd) {
      co_await onAgentcallEnd(in, result);
    }
  }

  asio::awaitable<void>
  onModelcallStartFunc(neograph::graph::NodeInput &in) override {
    if (nullptr != onModelcallStart) {
      co_await onModelcallStart(in);
    }
  }

  asio::awaitable<void>
  onModelcallEndFunc(const neograph::graph::NodeInput &in,
                     neograph::graph::NodeOutput &result) override {
    if (nullptr != onModelcallEnd) {
      co_await onModelcallEnd(in, result);
    }
  }

  asio::awaitable<void>
  onToolcallStartFunc(neograph::graph::NodeInput &in) override {
    if (nullptr != onToolcallStart) {
      co_await onToolcallStart(in);
    }
  }

  asio::awaitable<void>
  onToolcallEndFunc(const neograph::graph::NodeInput &in,
                    neograph::graph::NodeOutput &result) override {
    if (nullptr != onToolcallEnd) {
      co_await onToolcallEnd(in, result);
    }
  }
};

class MiddlewareWarpHandleContext {
public:
  inline static constexpr std::string graphDataKey_systemMessage{
      "systemMessage"};

  /// <thread_id, itemData>
  /// 每次执行的临时数据，在 [AgentStartCall] 时刷新，在 [AgentEndCall] 时清理
  std::map<std::string, std::map<std::string, std::any>> graphData{};
  std::vector<std::unique_ptr<BaseMiddlewareHandleInterface>> handles{};

  MiddlewareWarpHandleContext() {}

  template <typename T>
  T &getGraphDataItemValue(const std::string &thread_id,
                           const std::string &key) {
    auto &itemGraphData = graphData[thread_id];
    auto sysMsgIt = itemGraphData.find(key);
    if (sysMsgIt == itemGraphData.end()) {
      itemGraphData.insert(std::pair<std::string, std::any>{key, T{}});
    }
    return std::any_cast<T &>(itemGraphData[key]);
  }
};

} // namespace middleware
} // namespace agentxx