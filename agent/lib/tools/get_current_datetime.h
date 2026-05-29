#pragma once

#include "fmt/format.h"
#include "neograph/neograph.h"
#include <format>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace agentxx {
namespace tools {

/// 获取系统日期和时间
class GetCurrentDateTimeTool : public neograph::Tool {
public:
  explicit GetCurrentDateTimeTool() {}

  std::string get_name() const override { return "get_current_datetime"; }

  neograph::ChatTool get_definition() const override {
    return {
        "get_current_datetime",
        "Get current date and time.",
        neograph::json{},
    };
  }

  std::string execute(const neograph::json &arguments) override {
    auto now = std::chrono::system_clock::now();
    std::chrono::zoned_time local_time{std::chrono::current_zone(), now};

    return fmt::format(
        R"(
本地时间: {}
UTC 时间: {}
)",
        std::format("{:%Y-%m-%d} {:%H:%M:%S}", local_time, local_time),
        std::format("{:%Y-%m-%d} {:%H:%M:%S}", now, now));
  }
};

}; // namespace tools
}; // namespace agentxx