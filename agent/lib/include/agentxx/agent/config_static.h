#pragma once

#include <string>
#include <vector>

namespace agentxx {
namespace agent {

class AgentConfigStatic {
public:
  inline static constexpr std::string_view agentxxDataDirPath = ".agentxx";
};

} // namespace agent
} // namespace agentxx