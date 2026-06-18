#include "test_command_tools.h"
#include "test_datetime_tool.h"
#include "test_filesystem_tools.h"
#include "test_string_tools.h"
#include "test_web_search_tools.h"
#include "util/log.h"
#include <asio/awaitable.hpp>
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/io_context.hpp>
#include <asio/read.hpp>
#include <asio/redirect_error.hpp>
#include <asio/stream_file.hpp>
#include <asio/use_awaitable.hpp>
#include <fstream>
#include <iostream>
#include <map>

using namespace std;
asio::io_context ioCtx;

asio::awaitable<void> test(asio::io_context &ioCtx) {
  asio::stream_file stream{ioCtx.get_executor()};
  try {
    const auto filepath = std::string{
        "/home/coolight/program/agentxx/agent/lib/agent/deepagent.h"};
    stream.open(filepath, asio::stream_file::read_only);
    if (false == stream.is_open()) {
      stream.close();
      std::cout << "Can not open file." << std::endl;
      co_return;
    }

    std::string result;
    neograph_asio_error_code errCode;
    auto bytesReadLen = co_await asio::async_read(
        stream, asio::dynamic_buffer(result),
        asio::redirect_error(asio::use_awaitable, errCode));
    if (errCode != asio::error::eof) {
      throw std::system_error{errCode};
    }
    stream.close();
    auto readRange = std::string_view{result}.substr(0, bytesReadLen);
    std::cout << readRange << std::endl;
    co_return;
  } catch (const std::exception &e) {
    stream.close();
    XX_LOGD("FilesystemReadBinaryFileTool exception: {}", e.what());
    throw e;
  }
}

/// 运行可执行 ../script/test_run.sh
int main(int argn, char **argv) {
#if IS_LINUX_D
  agentxx::util::signalError(argv[0]);
#endif
  std::cout << "======= Test Start =======" << std::endl;

  asio::co_spawn(
      ioCtx,
      []() -> asio::awaitable<void> {
        co_await test(ioCtx);

        auto run = [](auto testFn) -> asio::awaitable<void> {
          try {
            co_await testFn();
          } catch (const std::exception &e) {
            std::cout << "[FAIL] Test suite exception: " << e.what()
                      << std::endl;
          }
        };

        co_await run(agentxx::test::run_string_tools_tests);
        co_await run(agentxx::test::run_datetime_tool_tests);
        co_await run(agentxx::test::run_filesystem_tools_tests);
        co_await run(agentxx::test::run_command_tools_tests);
        co_await run(agentxx::test::run_web_search_tools_tests);
      },
      asio::detached);
  ioCtx.run();

  std::cout << "======= Test Done =======" << std::endl;
  std::cout << ">>>";
  int num = 0;
  cin >> num;
  return 0;
}