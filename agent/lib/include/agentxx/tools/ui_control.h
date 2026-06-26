#pragma once

#include "agentxx/tools/tool.h"
#include <neograph/llm/rate_limited_provider.h>
#include <neograph/llm/schema_provider.h>
#include <neograph/neograph.h>
#include <string>

namespace agentxx {
namespace tools {

class UIControlKeyboardMouseTool : public XXToolBase {
public:
  explicit UIControlKeyboardMouseTool()
      : XXToolBase("ui_control_keyboard_mouse", false, true) {}

  neograph::ChatTool get_definition() const override;

  asio::awaitable<std::string>
  execute_async(const neograph::json &arguments) override;
};

} // namespace tools
} // namespace agentxx