#pragma once

#include "asio/io_context.hpp"
#include "fmt/base.h"
#include "fmt/format.h"
#include "middlewares/middleware.h"
#include <functional>
#include <neograph/llm/rate_limited_provider.h>
#include <neograph/llm/schema_provider.h>
#include <neograph/neograph.h>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace agentxx {
namespace tools {

class XXToolBase : public neograph::AsyncTool {
protected:
  const std::string name;

public:
  const bool canDelayLoad;

  explicit XXToolBase(const std::string &in_name, bool in_canDelayLoad)
      : name(in_name), canDelayLoad(in_canDelayLoad) {}

  std::string get_name() const override { return name; }

  virtual std::optional<agentxx::middleware::SummarizationToolHandle_c>
  createSummarizationToolHandle() const {
    return std::nullopt;
    // return agentxx::middleware::SummarizationToolHandle_c{
    //     .requestHandle =
    //         [](size_t index, std::map<std::string, size_t> &lastWriteIndex,
    //            neograph::json &args, neograph::ToolCall &toolcall) {

    //         },
    //     .responseHandle =
    //         [](size_t index, std::map<std::string, size_t> &lastWriteIndex,
    //            neograph::json &args, neograph::ChatMessage &msg) {

    //         },
    // };
  }
};

class XXToolWarp : public XXToolBase {
protected:
  std::unique_ptr<neograph::Tool> inner;
  std::optional<agentxx::middleware::SummarizationToolHandle_c>
      summarizationHandle;

public:
  explicit XXToolWarp(
      bool in_canDelayLoad, std::unique_ptr<neograph::Tool> &&in_inner,
      std::optional<agentxx::middleware::SummarizationToolHandle_c>
          in_summarizationHandle = std::nullopt)
      : XXToolBase("", in_canDelayLoad), inner(std::move(in_inner)),
        summarizationHandle(in_summarizationHandle) {}

  std::string get_name() const override { return inner->get_name(); }

  neograph::ChatTool get_definition() const override {
    return inner->get_definition();
  }

  asio::awaitable<std::string>
  execute_async(const neograph::json &arguments) override {
    co_return co_await inner->real_execute_async(arguments);
  }
};

} // namespace tools
} // namespace agentxx