#pragma once

#include <string>
#include <vector>

namespace agentxx {
class AgentxxConfig_c {
public:
  std::string agentName = "Agentxx";
  std::string agentNameView = "萝卜";

  std::string modelOpenAIBaseUrl = "";
  std::string modelOpenAIApiKey = "EMPTY";
  std::string modelOpenAIModelName = "Agentxx";

  std::string currentSystemName;
  std::string systemPrompt = "You are a helpful assistant.";
  std::vector<std::string> mcpServerUrls{};
};
} // namespace agentxx