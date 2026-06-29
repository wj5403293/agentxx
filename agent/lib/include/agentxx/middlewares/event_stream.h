#pragma once

#include "agentxx/middlewares/middleware.h"
#include "asio/io_context.hpp"
#include "fmt/format.h"
#include <asio/steady_timer.hpp>
#include <asio/use_awaitable.hpp>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <iterator>
#include <neograph/neograph.h>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace agentxx {
namespace middleware {

/// 仅用于作为模版类型，实现 Event 时应当继承 [EventStreamBase]
class EventStreamInterface {
public:
  std::string name;

  EventStreamInterface(std::string_view in_name) : name(in_name) {}

  virtual ~EventStreamInterface() {}
};

template <typename _ARG_TYPE, typename _EXPAND_DATA_TYPE, typename _DATA_TYPE>
class EventListener {
public:
  int execHit = std::numeric_limits<int>::max();
  std::shared_ptr<_EXPAND_DATA_TYPE> expand = nullptr;

  _ARG_TYPE arg;
  std::function<asio::awaitable<bool>(const _DATA_TYPE &)> handle;
};

template <typename _ARG_TYPE, typename _EXPAND_DATA_TYPE, typename _DATA_TYPE>
class EventStreamBase : public EventStreamInterface {
protected:
  size_t insertId = 0;

  /// <id, handle>
  std::map<size_t, EventListener<_ARG_TYPE, _EXPAND_DATA_TYPE, _DATA_TYPE>>
      listeners;

public:
  EventStreamBase(std::string_view in_name) : EventStreamInterface(in_name) {}

  size_t addListener(
      EventListener<_ARG_TYPE, _EXPAND_DATA_TYPE, _DATA_TYPE> &&listener) {
    assert(listener.execHit >= 1);
    auto id = ++insertId;
    listeners[id] = listener;
    onAddListener(listener);
    return id;
  }

  std::optional<std::map<size_t, EventListener<_ARG_TYPE, _EXPAND_DATA_TYPE,
                                               _DATA_TYPE>>::iterator>
  removeListener(size_t id) {
    auto it = listeners.find(id);
    if (it != listeners.end()) {
      return listeners.erase(it);
    }
    return listeners.end();
  }

  asio::awaitable<void> notify(const _DATA_TYPE &data) {
    auto list = std::vector<asio::awaitable<void>>{};
    for (auto it = listeners.begin(); it != listeners.end();) {
      it->execHit--;
      if (it->execHit <= 0) {
        auto next = removeListener(it->execHit);
        if (next.has_values()) {
          it = next.value();
        }
      }
      list.push_back(execHandle(*it, data));
      if (it != listeners.end()) {
        ++it;
      } else {
        break;
      }
    }
    for (auto &item : list) {
      co_await std::move(item);
    }
  }

  asio::awaitable<void>
  notifyItem(EventListener<_ARG_TYPE, _EXPAND_DATA_TYPE, _DATA_TYPE> &listener,
             const _DATA_TYPE &data) {
    listener.execHit--;
    if (listener.execHit <= 0) {
      removeListener(listener.execHit);
    }
    co_await execHandle(listener, data);
  }

  virtual asio::awaitable<void>
  execHandle(EventListener<_ARG_TYPE, _EXPAND_DATA_TYPE, _DATA_TYPE> &listener,
             const _DATA_TYPE &data) {
    co_await listener.handle(data);
  }

  virtual asio::awaitable<void> onAddListener(
      EventListener<_ARG_TYPE, _EXPAND_DATA_TYPE, _DATA_TYPE> &&listener) {}
};

class TimerEventStream
    : public EventStreamBase<size_t, asio::steady_timer, bool> {
public:
  inline static std::string defEventName = "xx_Timer";

  /// <delay:timestamp MS, _>
  TimerEventStream() : EventStreamBase(defEventName) {}

  asio::awaitable<void> onAddListener(
      EventListener<size_t, asio::steady_timer, bool> &&listener) override {
    assert(nullptr == listener.expand);
    auto timer = std::make_shared<asio::steady_timer>(
        co_await asio::this_coro::executor);
    timer->expires_after(std::chrono::milliseconds(listener.arg));
    listener.expand = timer;
    co_await timer->async_wait(asio::use_awaitable);
    notifyItem(listener, true);
  }
};

class DatetimeEventStream
    : public EventStreamBase<size_t, asio::steady_timer, bool> {
public:
  inline static std::string defEventName = "xx_Datetime";

  /// <datetime:timestamp MS, _>
  DatetimeEventStream() : EventStreamBase(defEventName) {}

  asio::awaitable<void> onAddListener(
      EventListener<size_t, asio::steady_timer, bool> &&listener) override {
    // TODO: 多轮短定时轮询，模拟长定时器
    assert(listener.execHit == 1);
    assert(nullptr == listener.expand);
    auto timer = asio::steady_timer{co_await asio::this_coro::executor};
    timer.expires_after(std::chrono::milliseconds(listener.arg));
    co_await timer.async_wait(asio::use_awaitable);
    notifyItem(listener, true);
  }
};

class EventStreamMiddlewareState : public BaseMiddlewareState {
public:
  EventStreamMiddlewareState() {}
};

class EventStreamMiddlewareHandle
    : public BaseMiddlewareHandle<EventStreamMiddlewareState> {
protected:
public:
  /// <name, handle>
  std::map<std::string, std::shared_ptr<EventStreamInterface>> events{};

  EventStreamMiddlewareHandle(
      std::weak_ptr<agentxx::agent::AgentContext> in_agentContext)
      : BaseMiddlewareHandle<EventStreamMiddlewareState>(
            "EventStreamMiddlewareHandle", in_agentContext) {}

  void addEvent(std::shared_ptr<EventStreamInterface> event) {
    events[event->name] = event;
  }
};

} // namespace middleware
} // namespace agentxx