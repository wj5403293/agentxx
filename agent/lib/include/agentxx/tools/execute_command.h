#pragma once

#include "agentxx/tools/tool.h"
#include "agentxx/util/string_util.h"
#include "agentxx/util/util.h"
#include "boost/asio/dispatch.hpp"
#include "boost/asio/io_context.hpp"
#include "boost/asio/read.hpp"
#include "boost/asio/readable_pipe.hpp"
#include "boost/asio/redirect_error.hpp"
#include "boost/asio/use_awaitable.hpp"
#include "boost/process.hpp"
#include "fmt/format.h"
#include <cstdlib>
#include <neograph/llm/rate_limited_provider.h>
#include <neograph/llm/schema_provider.h>
#include <neograph/neograph.h>
#include <sstream>
#include <string>
#include <vector>

namespace asio = ::boost::asio;

namespace agentxx {
namespace tools {

class ExecuteLinuxCommandTool : public XXToolBase {
public:
  ExecuteLinuxCommandTool(
      std::weak_ptr<agentxx::agent::AgentContext> in_agentContext)
      : XXToolBase("execute_linux_command", in_agentContext, true, false) {}

  neograph::ChatTool get_definition() const override {
    return {
        "execute_linux_command",
        "Run linux shell commands in the terminal.",
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
                             fmt::format(R"(Command to execute.
Current System is {}{}, please use linux shell/bash commands.)",
                                         agentxx::util::getSystemName(),
                                         agentxx::util::isRunningInWSL()
                                             ? "/(WSL)"
                                             : "")},
                        },
                    },
                    {
                        "all_output",
                        {
                            {"type", "boolean"},
                            {"description",
                             R"(Default `true`. 
`true`: Return all output.
`false`: Truncate Output. Only return the stdout and stderr output when the command faild.)"},
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
    auto all_output = arguments.value("all_output", true);

#if defined(BOOST_PROCESS_V2_PROCESS_HPP)
    {
      auto ctx = co_await asio::this_coro::executor;
      asio::readable_pipe outpip{ctx}, errpip{ctx};
      // 2. 创建管道，用于接收子进程的输出
      std::unordered_map<boost::process::environment::key,
                         boost::process::environment::value>
          procEnv;
      for (const auto &kv : boost::process::environment::current()) {
        if (kv.key().string() != "SECRET") {
          procEnv[kv.key()] = kv.value();
        }
      }

      auto proc = boost::process::process{
          ctx,
          boost::process::environment::find_executable("bash"),
          {"-c", command},
          boost::process::process_environment(procEnv),
          boost::process::process_stdio {
            .out = outpip,
            .err = errpip
          }};

      std::string strout, strerr;
      neograph_asio_error_code errCode;
      auto readStdOutFuture = asio::async_read(
          outpip, asio::dynamic_buffer(strout), asio::transfer_all(),
          asio::redirect_error(asio::use_awaitable, errCode));
      auto readStdErrFuture = asio::async_read(
          errpip, asio::dynamic_buffer(strerr), asio::transfer_all(),
          asio::redirect_error(asio::use_awaitable, errCode));
      // assert(!ec || (ec == asio::error::eof));
      co_await proc.async_wait();
      co_await std::move(readStdOutFuture);
      co_await std::move(readStdErrFuture);

      const auto exitCode = proc.exit_code();
      std::ostringstream result;
      result << "## ExitCode: " << exitCode << std::endl;
      if (all_output || 0 != exitCode) {
        // failed
        std::string encoding, str;
        if (strout.empty() ||
            agentxx::util::autoConvertToUtf8(strout, encoding, str)) {
          result << "## StdOut:\n" << str << std::endl;
        } else {
          result << "## StdOut convert to utf8 faild, truncated" << std::endl;
        }
        if (strerr.empty() ||
            agentxx::util::autoConvertToUtf8(strerr, encoding, str)) {
          result << "## StdErr:\n" << str << std::endl;
        } else {
          result << "## StdErr convert to utf8 faild, truncated" << std::endl;
        }
      }
      co_return result.str();
    }
#endif

#if XX_IS_WIN_D
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

    // TODO: 压缩
    // TODO: 太长则暂存到 share_store
    // TODO: 超长时存入文件，不在内存逗留
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
  ExecuteWindowsCommandTool(
      std::weak_ptr<agentxx::agent::AgentContext> in_agentContext)
      : XXToolBase("execute_windows_command", in_agentContext, true, false) {}

  neograph::ChatTool get_definition() const override{return {
    "execute_windows_command",
    R"(Run windows commands in the terminal.)",
    neograph::json{
        {"type", "object"},
        {
            "properties",
            {
                {
#if defined(BOOST_PROCESS_V2_PROCESS_HPP)
                    "command",
                    {
                        {"type", "string"},
                        {"description",
                         fmt::format(
                             R"({}

`command` 会传递到 `cmd.exe`执行，因此不需要你添加 `cmd.exe /c`

## Example:

- `powershell.exe`: PowerShell 命令行
- `explorer.exe`: 文件资源管理器
    - `explorer.exe path`: open windows explorer.exe and jump `path`
- `Taskmgr.exe`: 任务管理器，查看和管理正在运行的程序、进程和服务
- `Control.exe`: 控制面板
- `regedit.exe`: 注册表编辑器
- `calc.exe`: 计算器
- `notepad.exe`: 纯文本编辑器)",
                             agentxx::util::isRunningInWSL()
                                 ? R"(Command to execute, run in Linux(WSL)/Shell.
Current system is WSL, but can use this tool to execute windows command through cmd.exe.
Arg `command` is actually runs inside the windows terminal.)"
                                 : "Windows command to execute")},
                    },
                },
#else
                    "command",
                    {
                        {"type", "string"},
                        {"description",
                         fmt::format(
                             R"({}

Example:
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
- `notepad.exe`: 纯文本编辑器)",
                             agentxx::util::isRunningInWSL()
                                 ? R"(Command to execute, run in Linux(WSL)/Shell.
Windows Command must be executed through `cmd.exe`. Write arg command: `cmd.exe /c "win_cmd_str"`.
- Current system is WSL, but can use this tool to execute windows command through cmd.exe, there are some notes:
    - Arg `command`(e.g. `cmd.exe /c "{win_cmd_str}"`) is executed in the Linux/WSL shell. However, the `win_cmd_str` actually runs inside the windows terminal.
    - All win_cmd_str must be executed through cmd.exe (`cmd.exe /c "{win_cmd_str}"`).)"
                                 : "Windows command to execute")},
                    },
                },
#endif
                {
                    "all_output",
                    {
                        {"type", "boolean"},
                        {"description",
                         R"(Default `true`. 
`true`: Return all output.
`false`: Truncate Output. Only return the stdout and stderr output when the command faild.)"},
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
  auto all_output = arguments.value("all_output", true);
// TODO: UNC 路径不受支持。默认值设为 Windows 目录
#if defined(BOOST_PROCESS_V2_PROCESS_HPP)
  {
    auto ctx = co_await asio::this_coro::executor;
    asio::readable_pipe outpip{ctx}, errpip{ctx};
    // 2. 创建管道，用于接收子进程的输出
    std::unordered_map<boost::process::environment::key,
                       boost::process::environment::value>
        procEnv;
    for (const auto &kv : boost::process::environment::current()) {
      if (kv.key().string() != "SECRET") {
        procEnv[kv.key()] = kv.value();
      }
    }

    auto proc = boost::process::process{
        ctx,
        boost::process::environment::find_executable("cmd.exe"),
        {"/c", command},
        boost::process::process_environment(procEnv),
        boost::process::process_stdio{.out = outpip, .err = errpip}};

    std::string strout, strerr;
    neograph_asio_error_code errCode;
    auto readStdOutFuture = asio::async_read(
        outpip, asio::dynamic_buffer(strout), asio::transfer_all(),
        asio::redirect_error(asio::use_awaitable, errCode));
    auto readStdErrFuture = asio::async_read(
        errpip, asio::dynamic_buffer(strerr), asio::transfer_all(),
        asio::redirect_error(asio::use_awaitable, errCode));
    // assert(!ec || (ec == asio::error::eof));
    co_await proc.async_wait();
    co_await std::move(readStdOutFuture);
    co_await std::move(readStdErrFuture);

    const auto exitCode = proc.exit_code();
    std::ostringstream result;
    result << "## ExitCode: " << exitCode << std::endl;
    if (all_output || 0 != exitCode) {
      // failed
      std::string encoding, str;
      if (strout.empty() ||
          agentxx::util::autoConvertToUtf8(strout, encoding, str)) {
        result << "## StdOut:\n" << str << std::endl;
      } else {
        result << "## StdOut convert to utf8 faild, truncated" << std::endl;
      }
      if (strerr.empty() ||
          agentxx::util::autoConvertToUtf8(strerr, encoding, str)) {
        result << "## StdErr:\n" << str << std::endl;
      } else {
        result << "## StdErr convert to utf8 faild, truncated" << std::endl;
      }
    }
    co_return result.str();
  }
#endif

#if XX_IS_WIN_D
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
  if (agentxx::util::autoConvertToUtf8(result, encoding, utf8result)) {
    // 转 utf8
    co_return utf8result;
  }
  co_return result;
}
}; // namespace tools

class ExecutePythonTool : public XXToolBase {
public:
  ExecutePythonTool(std::weak_ptr<agentxx::agent::AgentContext> in_agentContext)
      : XXToolBase("execute_python", in_agentContext, true, false) {}

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
  ExecuteJavaScriptTool(
      std::weak_ptr<agentxx::agent::AgentContext> in_agentContext)
      : XXToolBase("execute_javascript", in_agentContext, true, false) {}

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

} // namespace agentxx
} // namespace agentxx