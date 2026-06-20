#include "middleware.h"
#include "tools/tool.h"

agentxx::middleware::BaseMiddlewareHandleInterface::
    BaseMiddlewareHandleInterface(
        const std::string &in_name,
        std::weak_ptr<agentxx::agent::AgentContext> in_agentContext)
    : name(in_name), agentContext(in_agentContext) {}

asio::awaitable<std::optional<neograph::json>>
agentxx::middleware::MiddlewareWarpContext::execInterruptHandle(
    const std::string &name, agentxx::middleware::InterruptHandleArg &arg) {
  auto handleIt = interruptHandles.find(arg.name);
  if (handleIt != interruptHandles.end()) {
    auto resumeValue = co_await handleIt->second(arg);
    co_return resumeValue;
  }
  co_return std::nullopt;
}