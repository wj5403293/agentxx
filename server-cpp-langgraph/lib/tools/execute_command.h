#pragma once

#include <cstdlib>
#include <neograph/graph/deep_research_graph.h>
#include <neograph/llm/rate_limited_provider.h>
#include <neograph/llm/schema_provider.h>
#include <neograph/neograph.h>
#include <sstream>
#include <string>
#include <vector>

namespace agentxx {
namespace tools {

class ExecuteCommandTool : public neograph::AsyncTool {
public:
  explicit ExecuteCommandTool() {}

  std::string get_name() const override { return "execute_command"; }

  neograph::ChatTool get_definition() const override {
    return {
        "execute_command",
        R"(Run shell commands in the environment.
Current System is Linux, please use shell/bash.
)",
        neograph::json{
            {"type", "object"},
            {
                "properties",
                {
                    {
                        "command",
                        {
                            {"type", "string"},
                            {"description", "Command to execute"},
                        },
                    },
                },
            },
            {"required", neograph::json::array({"command"})},
        },
    };
  }

  asio::awaitable<std::string>
  execute_async(const neograph::json &arguments) override {
    auto command = arguments.value("command", std::string{});
    if (command.empty()) {
      co_return R"({"error":"Arg `command` is empty"})";
    }

    // TODO: async
#if IS_WIN_D
    auto pipe = std::unique_ptr<FILE, decltype(&_pclose)>{
        _popen(command.c_str(), "r"), _pclose};
#else
    auto pipe = std::unique_ptr<FILE, decltype(&pclose)>{
        popen(command.c_str(), "r"), pclose};
#endif
    if (!pipe) {
      auto ec = std::error_code{errno, std::system_category()};
      co_return std::format(R"({{"error":"Exec command faild. Error: {}"}})",
                            ec.message());
    }

    std::array<char, 1024> buffer{};
    std::ostringstream result;
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
      result << buffer.data();
    }
    co_return result.str();
  }
};
} // namespace tools
} // namespace agentxx