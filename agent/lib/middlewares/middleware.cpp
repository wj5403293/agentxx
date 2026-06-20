#include "middleware.h"
#include "tools/tool.h"

agentxx::middleware::BaseMiddlewareHandleInterface::
    BaseMiddlewareHandleInterface(
        const std::string &in_name,
        std::weak_ptr<agentxx::middleware::MiddlewareWarpHandleContext>
            in_handleContext)
    : name(in_name), handleContext(in_handleContext) {}

asio::awaitable<std::optional<neograph::json>>
agentxx::middleware::MiddlewareWarpHandleContext::execInterruptHandle(
    const std::string &name, agentxx::middleware::InterruptHandleArg_c &arg) {
  auto handleIt = interruptHandles.find(arg.name);
  if (handleIt != interruptHandles.end()) {
    auto resumeValue = co_await handleIt->second(arg);
    co_return resumeValue;
  }
  co_return std::nullopt;
}