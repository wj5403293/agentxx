#pragma once

#include "neograph/neograph.h"

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"

#include <format>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace agentxx {
namespace tools {

/// 获取系统日期和时间
class GetCurrentSystemDateTimeTool : public neograph::Tool {
public:
  explicit GetCurrentSystemDateTimeTool() {}

  std::string get_name() const override {
    return "get_current_system_datetime";
  }

  neograph::ChatTool get_definition() const override {
    return {
        "get_current_system_datetime",
        "Get current system date and time.",
        neograph::json{},
    };
  }

  std::string execute(const neograph::json &arguments) override {
    auto now = std::chrono::system_clock::now();
    std::chrono::zoned_time local_time{std::chrono::current_zone(), now};

    return std::format(R"(
本地时间: {}
UTC 时间: {}
)",
                       std::format("{:%Y-%m-%d %H:%M:%S}", local_time),
                       std::format("{:%Y-%m-%d %H:%M:%S}", now));
  }
};

}; // namespace tools
}; // namespace agentxx