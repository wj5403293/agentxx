#pragma once

#include "asio/io_context.hpp"
#include "fmt/format.h"
#include "tools/tool.h"
#include "util/string_util.h"
#include "util/util.h"
#include <cstdlib>
#include <neograph/llm/rate_limited_provider.h>
#include <neograph/llm/schema_provider.h>
#include <neograph/neograph.h>
#include <sstream>
#include <string>
#include <vector>

namespace agentxx {
namespace tools {

class ExecuteLinuxCommandTool : public XXToolBase {
public:
  explicit ExecuteLinuxCommandTool()
      : XXToolBase("execute_linux_command", false) {}

  neograph::ChatTool get_definition() const override {
    return {
        "execute_linux_command",
        fmt::format(R"(Run linux shell commands in the terminal.
Current System is {}{}, please use linux shell/bash commands.
)",
                    agentxx::util::getSystemName(),
                    agentxx::util::isRunningInWSL() ? "/(WSL)" : ""),
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
      co_return fmt::format(R"({{"error":"Exec command failed. Error: {}"}})",
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

/// windows cmd
/// 支持 WSL
class ExecuteWindowsCommandTool : public XXToolBase {
public:
  explicit ExecuteWindowsCommandTool()
      : XXToolBase("execute_windows_command", false) {}

  neograph::ChatTool get_definition() const override {
    return {
        "execute_windows_command",
        fmt::format(
            R"(Run windows commands in the terminal.
Example:
{}
- `cmd.exe`: use windows CMD to run commands. 命令行/终端.
    - `cmd.exe /c "win_cmd_str"`: run `win_cmd_str` in windows terminal
    - `cmd.exe /c "echo hello"`
    - `cmd.exe /c "mkdir test"`
- `powershell.exe`: PowerShell 命令行
- `explorer.exe`: 文件资源管理器
    - `explorer.exe path`: open windows explorer.exe and jump `path`
- `Taskmgr.exe`: 任务管理器，查看和管理正在运行的程序、进程和服务
- `Control.exe`: 控制面板
- `regedit.exe`: 注册表编辑器
- `calc.exe`: 计算器
- `notepad.exe`: 纯文本编辑器
)",
            agentxx::util::isRunningInWSL() ? R"(
- Current system is WSL, but can use this tool to execute windows command through cmd.exe, there are some notes:
    - Arg `command`(e.g. `cmd.exe /c "{win_cmd_str}"`) is executed in the Linux/WSL shell. However, the `win_cmd_str` actually runs inside the windows terminal.
    - All win_cmd_str must be executed through cmd.exe (`cmd.exe /c "{win_cmd_str}"`).
)"
                                            : ""),
        neograph::json{
            {"type", "object"},
            {
                "properties",
                {
                    {
                        "command",
                        {
                            {"type", "string"},
                            {"description",
                             agentxx::util::isRunningInWSL()
                                 ? R"(Command to execute, run in Linux(WSL)/Shell. 
Windows Command must be executed through `cmd.exe`. Write arg command: `cmd.exe /c "win_cmd_str"`)"
                                 : "Windows command to execute"},
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
      co_return fmt::format(R"({{"error":"Exec command failed. Error: {}"}})",
                            ec.message());
    }

    std::array<char, 1024> buffer{};
    std::ostringstream out{};
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
      out << buffer.data();
    }
    std::string result = out.str();
    std::string encoding, utf8result;
    if (agentxx::util::chardetConvertEncoding(result, encoding, utf8result)) {
      // 转 utf8
      co_return utf8result;
    }
    co_return result;
  }
};

class ExecutePythonTool : public XXToolBase {
public:
  explicit ExecutePythonTool() : XXToolBase("execute_python", false) {}

  neograph::ChatTool get_definition() const override {
    return {
        "execute_python",
        "Run python commands in the terminal.",
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

    co_return "";
  }
};

class ExecuteJavaScriptTool : public XXToolBase {
public:
  explicit ExecuteJavaScriptTool() : XXToolBase("execute_javascript", false) {}

  neograph::ChatTool get_definition() const override {
    return {
        "execute_javascript",
        "Run javascript commands in the terminal.",
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

    co_return "";
  }
};

} // namespace tools
} // namespace agentxx