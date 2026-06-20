#pragma once

#include "asio/io_context.hpp"
#include "fmt/format.h"
#include "middlewares/middleware.h"
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <neograph/neograph.h>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace agentxx {
namespace middleware {

class PermissionMiddlewareState : public BaseMiddlewareState {
public:
  std::map<std::string, neograph::json> handles{};

  PermissionMiddlewareState() {}
};

class PermissionMiddlewareHandle
    : public BaseMiddlewareHandle<PermissionMiddlewareState> {
protected:
public:
  PermissionMiddlewareHandle(
      std::weak_ptr<agentxx::agent::AgentContext> in_agentContext)
      : BaseMiddlewareHandle<PermissionMiddlewareState>(
            "PermissionMiddlewareHandle", in_agentContext) {
    registerInterruptHandles();
  }

  inline static constexpr std::string handleName_default = "default";

  void registerInterruptHandles() {
    auto agentCtxPtr = agentContext.lock();
    agentCtxPtr->middlewareHandleContext->interruptHandles[handleName_default] =
        [](const agentxx::middleware::InterruptHandleArg &handleArg)
        -> asio::awaitable<neograph::json> {
      auto result = neograph::json::array();
      std::cout << "\n  ┏━━━━━━ Input ━━━━━━┓" << std::endl;
      bool haveWaitInput = false;
      for (const auto &input : handleArg.inputs) {
        bool inputSuccess = false;
        do {
          std::cout << fmt::format("  ┣━ ## {} : {}", input.label, input.depict)
                    << std::endl;

          if (input.type.empty()) {
            // 不需要输入
            inputSuccess = true;
          } else {
            std::cout << "  ┣━ Type | " << input.type << " | ";
            if ("bool" == input.type) {
              std::cout << "`yes/y` or `no/n`";
            } else if ("int" == input.type) {
            } else if ("double" == input.type) {
            } else if ("string" == input.type) {
            } else if ("enum" == input.type) {
              std::cout << "value of [";
              for (const auto &val : input.enumValues) {
                std::cout << val << ", ";
              }
              std::cout << "]";
            }
            std::cout << std::endl;
            std::cout << "  ┣━ Default Value: " << input.defaultValue
                      << std::endl;
            std::cout << "  ┣━ >>> " << std::flush;

            haveWaitInput = true;
            std::string line;
            std::getline(std::cin, line);
            if (line.empty()) {
              line = input.defaultValue;
            }

            if ("bool" == input.type) {
              std::transform(line.begin(), line.end(), line.begin(),
                             [](unsigned char c) { return std::tolower(c); });
              if (line == "yes" || line == "y") {
                line = "true";
                inputSuccess = true;
              } else if (line == "no" || line == "n") {
                line = "false";
                inputSuccess = true;
              } else {
                inputSuccess = false;
              }
            } else if ("int" == input.type) {
              long long num;
              auto result = std::from_chars(line.c_str(),
                                            line.c_str() + line.size(), num);
              inputSuccess = (result.ec == std::errc{});
            } else if ("double" == input.type) {
              double num;
              auto result = std::from_chars(line.c_str(),
                                            line.c_str() + line.size(), num);
              inputSuccess = (result.ec == std::errc{});
            } else if ("string" == input.type) {
              inputSuccess = true;
            } else if ("enum" == input.type) {
              for (const auto &val : input.enumValues) {
                if (val == line) {
                  inputSuccess = true;
                  break;
                }
              }
            }

            if (inputSuccess) {
              result.push_back(line);
            } else {
              std::cout << "  ┣━ Invalid Input, please try again." << std::endl;
            }
          }
        } while (false == inputSuccess);
      }
      if (false == haveWaitInput) {
        // 没有输入，等待用户确认
        std::cout << "  ┣━ Wait user review, `Enter` to continue." << std::endl;
        std::cout << "  ┣━ >>> " << std::flush;
        std::string temp;
        std::getline(std::cin, temp);
      }
      std::cout << "  ┗━━━━━━ Input ━━━━━━┛\n" << std::endl;
      co_return result;
    };
  }
};
} // namespace middleware
} // namespace agentxx