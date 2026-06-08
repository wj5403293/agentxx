#pragma once

#include <filesystem>
#include <format>
#include <iostream>
#include <map>
#include <memory>
#include <neograph/llm/rate_limited_provider.h>
#include <neograph/llm/schema_provider.h>
#include <neograph/neograph.h>
#include <sstream>
#include <string>
#include <vector>

namespace agentxx {
namespace tools {

/// 寄存信息，节省模型上下文
/// TODO: 重启恢复
class TempTextStoreTool : public neograph::Tool {
protected:
  /// <thread_id, <id, value>>
  std::map<std::string, std::map<int, std::string>> store{};

public:
  explicit TempTextStoreTool() {}

  std::string get_name() const override { return "temp_text_store"; }

  neograph::ChatTool get_definition() const override {
    return {
        "temp_text_store",
        R"(Store text in memory, . Return a unique id, used to get text when need.
Insert text or get/set/delete text by unique id.
)",
        neograph::json{
            {"type", "object"},
            {
                "properties",
                {
                    {
                        "opt",
                        {
                            {"type", "string"},
                            {"enum", neograph::json::array({
                                         "get",
                                         "insert",
                                         "set",
                                         "delete",
                                     })},
                            {"description", R"(operation.
`get` Get text by unique id
`insert` New store `text`, return unique id
`set` Modify `text` by unique id
`delete` Delete text by unique id
)"},
                        },
                    },
                    {
                        "text",
                        {
                            {"type", "string"},
                            {"description", "text content to store in memory"},
                        },
                    },
                    {
                        "id",
                        {
                            {"type", "string"},
                            {"description", "unique id to store text when opt "
                                            "is `get`,`set` or `delete`"},
                        },
                    },
                },
            },
            {"required", neograph::json::array({"opt"})},
        },
    };
  }

  std::string execute(const neograph::json &arguments) override {
    auto text = arguments.value("text", std::string{});
    if (text.empty()) {
      return R"({"error":"Arg `text` is empty"})";
    }
    auto text_id = arguments.value("id", std::string{});
    auto text_opt = arguments.value("opt", std::string{});

    if (text_opt == std::string_view{"insert"}) {
    } else if (text_opt == std::string_view{"get"}) {
    } else if (text_opt == std::string_view{"set"}) {
    } else if (text_opt == std::string_view{"delete"}) {
    } else {
      return R"({"error":"Arg `opt` is invalid"})";
    }
  }
};
} // namespace tools
} // namespace agentxx