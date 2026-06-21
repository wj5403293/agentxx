#pragma once

#include <string>
#include <vector>

namespace agentxx {
namespace agent {

class AgentConfig {
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
  std::vector<std::string> ragDocsPaths{};

  /// LLM 节点最大重试次数
  /// - 最多执行 1 + 3(retry) = 4 次
  size_t llmRetry = 3;

  /// TODO: 更换api
  /// - [duckduckgo] `https://duckduckgo.com/html/?q={}` 国内连接不稳定
  std::string websearchApiUrl = "";
  bool websearchConvertHtml2markdown = true;

  bool logPrintMessagesBeforeLLM = false;
};

} // namespace agent
} // namespace agentxx