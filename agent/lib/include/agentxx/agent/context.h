#pragma once

#include "agentxx/agent/config.h"
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace agentxx {
namespace middleware {
class MiddlewareContext;
class PermissionMiddlewareHandle;
} // namespace middleware

namespace tools {
class SubAgentManagerTool;
}

namespace agent {

class AgentContext {
public:
  std::shared_ptr<agentxx::agent::AgentConfig> agentConfig = nullptr;
  std::shared_ptr<agentxx::middleware::MiddlewareContext>
      middlewareHandleContext = nullptr;
  std::shared_ptr<agentxx::middleware::PermissionMiddlewareHandle>
      permissionMiddleware;
  agentxx::tools::SubAgentManagerTool *subagentManagerToolPtr = nullptr;
};
} // namespace agent
} // namespace agentxx