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

class SkillTool : public neograph::AsyncTool {
protected:
public:
  explicit SkillTool() {}

  std::string get_name() const override { return "skill_tool"; }

  neograph::ChatTool get_definition() const override {
    return {
        "skill_tool",
        R"(Store text in memory. Return a unique id, used to get text by toolcall `memory_deposit_get`.
New store text or Set/Delete text by unique id.
)",
        neograph::json{
            {"type", "object"},
            {
                "properties",
                {
                    {
                        "path",
                        {
                            {"type", "string"},
                            {"description", "text content to store in memory"},
                        },
                    },
                    {
                        "opt",
                        {
                            {"type", "string"},
                            {"enum", neograph::json::array({
                                         "load",
                                         "offload",
                                     })},
                            {"description", R"(operation.
`load` New store text, return unique id
`offload` Modify text by unique id
)"},
                        },
                    },
                },
            },
            {"required", neograph::json::array({"text"})},
        },
    };
  }

  asio::awaitable<std::string>
  execute_async(const neograph::json &arguments) override {
    auto text = arguments.value("text", std::string{});
    if (text.empty()) {
      co_return R"({"error":"Arg `text` is empty"})";
    }
    auto text_id = arguments.value("id", std::string{});
    auto text_opt = arguments.value("opt", std::string{});

    if (text_opt == std::string_view{"insert"}) {
    } else if (text_opt == std::string_view{"set"}) {
    } else if (text_opt == std::string_view{"delete"}) {
    } else {
      co_return R"({"error":"Arg `opt` is invalid"})";
    }
  }
};
} // namespace tools
} // namespace agentxx