#include "util/log.h"
#include <asio/awaitable.hpp>
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
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
    asio::error_code errCode;
    auto bytesReadLen = co_await asio::async_read(
        stream, asio::dynamic_buffer(result),
        asio::redirect_error(asio::use_awaitable, errCode));
    if (errCode != asio::error::eof) {
      throw asio::system_error{errCode};
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

int main(int argn, char **argv) {
#if IS_LINUX_D
  agentxx::util::signalError(argv[0]);
#endif
  std::cout << "======= Test Start =======" << std::endl;

  asio::co_spawn(
      ioCtx, []() -> asio::awaitable<void> { co_return co_await test(ioCtx); },
      asio::detached);
  ioCtx.run();

  std::cout << "======= Test Done =======" << std::endl;
  std::cout << ">>>";
  int num = 0;
  cin >> num;
  return 0;
}