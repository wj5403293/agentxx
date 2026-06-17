#pragma once

#include <neograph/llm/rate_limited_provider.h>
#include <neograph/llm/schema_provider.h>
#include <neograph/neograph.h>

#include "tools/tool.h"
#include "util/http_client.h"
#include "util/log.h"
#include <cstdlib>
#include <format>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace agentxx {
namespace tools {

class UIControlKeyboardMouseTool : public XXToolBase {
public:
  explicit UIControlKeyboardMouseTool()
      : XXToolBase("ui_control_keyboard_mouse", false, true) {}

  neograph::ChatTool get_definition() const override {
    return {
        "ui_control_keyboard_mouse",
        "网络搜索. 返回一个 markdown 列表结果. "
        "然后可以使用 `fetch_url_markdown` 拉取网页具体内容.",
        neograph::json{
            {"type", "object"},
            {
                "properties",
                {{
                    "query",
                    {
                        {"type", "string"},
                        {"description", "Search query."},
                    },
                }},
            },
            {"required", neograph::json::array({"query"})},
        },
    };
  }

  asio::awaitable<std::string>
  execute_async(const neograph::json &arguments) override {
    std::string query = arguments.value("query", std::string{});
    if (query.empty()) {
      co_return R"({"error":"Arg `query` is empty"})";
    }
    co_return "";
  }
};

} // namespace tools
} // namespace agentxx