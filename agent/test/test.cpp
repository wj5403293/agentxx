#include "agentxx/util/log.h"
#include "asio/awaitable.hpp"
#include "asio/co_spawn.hpp"
#include "asio/detached.hpp"
#include "asio/io_context.hpp"
#include "asio/random_access_file.hpp"
#include "asio/read.hpp"
#include "asio/read_at.hpp"
#include "asio/read_until.hpp"
#include "asio/redirect_error.hpp"
#include "asio/registered_buffer.hpp"
#include "asio/stream_file.hpp"
#include "asio/use_awaitable.hpp"
#include "test_command_tools.h"
#include "test_datetime_tool.h"
#include "test_filesystem_tools.h"
#include "test_string_tools.h"
#include "test_web_search_tools.h"
#include <fstream>
#include <iostream>
#include <map>

#if IS_WIN_D
#include <Windows.h>
#endif

asio::io_context ioCtx;

asio::awaitable<void> test(asio::io_context &ioCtx) { co_return; }

/// 运行可执行 ../script/test_run.sh
int main(int argn, char **argv) {
#if IS_WIN_D
  SetConsoleOutputCP(CP_UTF8);
#endif
#if IS_DEBUG_D && IS_LINUX_D
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
  std::cin >> num;
  return 0;
}