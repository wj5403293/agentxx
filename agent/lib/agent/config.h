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
  bool isSystemWSL = false;
  std::string systemPrompt = "You are a helpful assistant.";
  std::vector<std::string> skillDirPaths{};
  std::vector<std::string> mcpServerUrls{};

  /// TODO: 更换api
  /// - [duckduckgo] `https://duckduckgo.com/html/?q={}` 国内连接不稳定
  std::string websearchApiUrl = "";
  bool websearchConvertHtml2markdown = true;
};
} // namespace agentxx