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

namespace asio = ::boost::asio;

namespace agentxx {
namespace tools {
class XXToolBase;
class XXToolWarp;
} // namespace tools

namespace middleware {

using onGraphNodeBeforeCallFunc =
    std::function<asio::awaitable<void>(neograph::graph::NodeInput &in)>;
using onGraphNodeAfterCallFunc = std::function<asio::awaitable<void>(
    const neograph::graph::NodeInput &in, neograph::graph::NodeOutput &result)>;

class MiddlewareWarpHandleContext;

class BaseMiddlewareState_c {
public:
  BaseMiddlewareState_c() {}

  virtual ~BaseMiddlewareState_c() {}
};

template <typename T>
concept BaseMiddlewareStateType = std::same_as<T, BaseMiddlewareState_c> ||
                                  std::derived_from<T, BaseMiddlewareState_c>;

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
  std::vector<std::unique_ptr<agentxx::tools::XXToolBase>> toolcalls{};
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

  inline static const neograph::ChatMessage *getLastAssistantToolcallMessage(
      std::vector<neograph::ChatMessage> &messages) {
    const neograph::ChatMessage *assistant_msg = nullptr;
    if (false == messages.empty()) {
      for (auto it = messages.rbegin(); it != messages.rend(); ++it) {
        if (it->role == "assistant" && !it->tool_calls.empty()) {
          assistant_msg = &(*it);
          break;
        }
      }
    }
    return assistant_msg;
  }

  inline static const neograph::ChatMessage *
  getLastToolcallResultMessage(std::vector<neograph::ChatMessage> &messages) {
    const neograph::ChatMessage *tool_msg = nullptr;
    if (false == messages.empty()) {
      for (auto it = messages.rbegin(); it != messages.rend(); ++it) {
        if (it->role == "tool") {
          tool_msg = &(*it);
          break;
        }
      }
    }
    return tool_msg;
  }
};

template <BaseMiddlewareStateType T>
class BaseMiddlewareHandle : public BaseMiddlewareHandleInterface {
protected:
public:
  BaseMiddlewareHandle(
      const std::string &in_name,
      std::weak_ptr<MiddlewareWarpHandleContext> in_handleContext)
      : BaseMiddlewareHandleInterface(in_name, in_handleContext) {}

  asio::awaitable<void>
  onAgentcallStartFunc(neograph::graph::NodeInput &in) override {
    co_return;
  }

  asio::awaitable<void>
  onAgentcallEndFunc(const neograph::graph::NodeInput &in,
                     neograph::graph::NodeOutput &result) override {
    co_return;
  }

  asio::awaitable<void>
  onModelcallStartFunc(neograph::graph::NodeInput &in) override {
    co_return;
  }

  asio::awaitable<void>
  onModelcallEndFunc(const neograph::graph::NodeInput &in,
                     neograph::graph::NodeOutput &result) override {
    co_return;
  }

  asio::awaitable<void>
  onToolcallStartFunc(neograph::graph::NodeInput &in) override {
    co_return;
  }

  asio::awaitable<void>
  onToolcallEndFunc(const neograph::graph::NodeInput &in,
                    neograph::graph::NodeOutput &result) override {
    co_return;
  }

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
  class TempStore {
  public:
    std::map<size_t, std::string> store{};
    size_t tempStoreId = 1;

    size_t getNextTempStoreId() { return tempStoreId++; }
  };

  inline static constexpr std::string graphDataKey_systemMessage{
      "systemMessage"};

  /// <thread_id, <id, value>>
  /// - 存储变量内容，留出 id 到 上下文中，llm 需要时可以通过
  /// toolcall/share_store 读取
  /// - 如: 压缩上下文时会将部分长文本存入这里替换为 id
  std::map<std::string, TempStore> tempStore{};

  /// <thread_id, itemData>
  /// [会话独立] 每次执行的临时数据，在 [AgentStartCall] 时刷新，在
  /// [AgentEndCall] 时清理
  std::map<std::string, std::map<std::string, std::any>> graphData{};

  /// 用基类声明类型，以便支持插入不同子类
  /// - 中间的指针是必要的，直接写 std::vector<BaseMiddlewareHandleInterface>
  /// 的话元素大小是 固定为基类大小，插入子类时内存会被截断，导致后续异常
  std::vector<std::shared_ptr<BaseMiddlewareHandleInterface>> handles{};

  MiddlewareWarpHandleContext() {}

  std::optional<std::string> getTempStoreItemValue(const std::string &thread_id,
                                                   const int id) {
    auto it = tempStore.find(thread_id);
    if (tempStore.end() != it) {
      auto reslut = it->second.store.find(id);
      if (it->second.store.end() != reslut) {
        return reslut->second;
      }
    }
    return std::nullopt;
  }

  void setTempStoreItemValue(const std::string &thread_id, const int id,
                             const std::string &value) {
    tempStore[thread_id].store[id] = value;
  }

  size_t addTempStoreItemValue(const std::string &thread_id,
                               const std::string &value) {
    auto store = tempStore[thread_id];
    auto id = store.getNextTempStoreId();
    store.store[id] = value;
    return id;
  }

  void removeTempStoreItemValue(const std::string &thread_id, const int id) {
    auto it = tempStore.find(thread_id);
    if (tempStore.end() != it) {
      auto reslutIt = it->second.store.find(id);
      if (it->second.store.end() != reslutIt) {
        it->second.store.erase(reslutIt);
      }
    }
  }

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

class SummarizationToolHandle_c {
public:
  std::function<void(size_t index,
                     std::map<std::string, size_t> &lastWriteIndex,
                     neograph::json &, neograph::ToolCall &)>
      requestHandle;
  std::function<void(size_t index,
                     std::map<std::string, size_t> &lastWriteIndex,
                     neograph::json &, neograph::ChatMessage &)>
      responseHandle;
};

class InterruptHandleArg_c {
public:
  class InterruptHandleArgItem_c {
  public:
    std::string label;
    std::string depict;
    /// bool / int / double / enum / string
    std::string type;

    inline static InterruptHandleArgItem_c
    fromJson(const neograph::json &data) {
      auto result = InterruptHandleArgItem_c{};
      if (data.is_object()) {
        if (data["label"].is_string()) {
          result.label = data["label"].get<std::string>();
        }
        if (data["depict"].is_string()) {
          result.depict = data["depict"].get<std::string>();
        }
        if (data["type"].is_string()) {
          result.type = data["type"].get<std::string>();
        }
      }
      return result;
    }

    neograph::json toJson() const {
      return neograph::json{
          {"label", label},
          {"depict", depict},
          {"type", type},
      };
    }
  };

  std::string name;
  std::vector<InterruptHandleArgItem_c> args;

  void throwInterrupt() {
    throw neograph::graph::NodeInterrupt{toJson().dump()};
  }

  inline static InterruptHandleArg_c fromJson(const neograph::json &data) {
    auto result = InterruptHandleArg_c{};
    if (data.is_object()) {
      if (data["name"].is_string()) {
        result.name = data["name"].get<std::string>();
      }
      if (data["args"].is_array()) {
        for (const auto &arg : data["args"]) {
        }
      }
    }
    return result;
  }

  neograph::json toJson() const {
    auto argsJson = neograph::json::array();
    for (const auto &item : args) {
      argsJson.push_back(item.toJson());
    }
    return neograph::json{
        {"name", name},
        {"args", argsJson},
    };
  }
};

} // namespace middleware
} // namespace agentxx