#pragma once

#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace agentxx {
namespace middleware {
class MiddlewareWarpContext;
}

namespace tools {
class SubAgentManagerTool;
}

namespace agent {

class AgentConfig;

class AgentContext {
public:
  std::shared_ptr<agentxx::agent::AgentConfig> agentConfig = nullptr;
  std::shared_ptr<agentxx::middleware::MiddlewareWarpContext>
      middlewareHandleContext = nullptr;
  agentxx::tools::SubAgentManagerTool *subagentManagerToolPtr = nullptr;
};
} // namespace agent
} // namespace agentxx