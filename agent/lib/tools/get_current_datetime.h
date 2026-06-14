#pragma once

#include "fmt/format.h"
#include "neograph/neograph.h"
#include "tools/tool.h"
#include <format>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace agentxx {
namespace tools {

/// 获取系统日期和时间
class GetCurrentDateTimeTool : public XXToolBase {
public:
  explicit GetCurrentDateTimeTool()
      : XXToolBase("get_current_datetime", true) {}

  neograph::ChatTool get_definition() const override {
    return {
        "get_current_datetime",
        "Get current date,time and timestamp.",
        neograph::json{},
    };
  }

  asio::awaitable<std::string>
  execute_async(const neograph::json &arguments) override {
    auto now = std::chrono::system_clock::now();
    std::chrono::zoned_time local_time{std::chrono::current_zone(), now};

    co_return fmt::format(
        R"(Timestamp: {} ms
Local Time: {}
UTC Time: {})",
        static_cast<long long>(now.time_since_epoch().count() / 1000 / 1000),
        std::format("{:%Y-%m-%d} {:%H:%M:%S}", local_time, local_time),
        std::format("{:%Y-%m-%d} {:%H:%M:%S}", now, now));
  }
};

}; // namespace tools
}; // namespace agentxx