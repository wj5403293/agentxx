#pragma once

#include "middlewares/middleware.h"
#include <filesystem>
#include <format>
#include <iostream>
#include <map>
#include <memory>
#include <neograph/llm/rate_limited_provider.h>
#include <neograph/llm/schema_provider.h>
#include <neograph/neograph.h>
#include <sstream>
#include <string>
#include <vector>

namespace agentxx {
namespace tools {

/// 寄存信息，节省模型上下文
/// TODO: 重启恢复
class TempKVStoreTool : public neograph::Tool {
protected:
  std::weak_ptr<agentxx::middleware::MiddlewareWarpHandleContext> handleContext;

public:
  explicit TempKVStoreTool(
      std::weak_ptr<agentxx::middleware::MiddlewareWarpHandleContext>
          in_handleContext)
      : handleContext(in_handleContext) {}

  std::string get_name() const override { return "temp_kvstore"; }

  neograph::ChatTool get_definition() const override {
    return {
        "temp_kvstore",
        R"(Store text, return a unique id, used to get text when need.
Insert text or get/set/delete text by unique id.
)",
        neograph::json{
            {"type", "object"},
            {
                "properties",
                {
                    {
                        "opt",
                        {
                            {"type", "string"},
                            {"enum", neograph::json::array({
                                         "get",
                                         "insert",
                                         "set",
                                         "delete",
                                     })},
                            {"description", R"(operation.
`get` Get text by unique id
`insert` New store `text`, return unique id
`set` Modify `text` by unique id
`delete` Delete text by unique id
)"},
                        },
                    },
                    {
                        "text",
                        {
                            {"type", "string"},
                            {"description", "text content to store in memory"},
                        },
                    },
                    {
                        "id",
                        {
                            {"type", "number"},
                            {"description", "unique id to store text when opt "
                                            "is `get`,`set` or `delete`"},
                        },
                    },
                },
            },
            {"required", neograph::json::array({"opt"})},
        },
    };
  }

  std::string execute(const neograph::json &arguments) override {
    auto thread_id = arguments.value("thread_id", std::string{});
    if (thread_id.empty()) {
      return R"({"error":"Toolcall inner exec faild, need `thread_id`"})";
    }
    size_t text_id = arguments.value<size_t>("id", 0);
    auto text = arguments.value("text", std::string{});
    auto text_opt = arguments.value("opt", std::string{});
    if (text_opt.empty()) {
      return R"({"error":"Arg `opt` is empty"})";
    }

    auto handlePtr = handleContext.lock();
    if (text_opt == std::string_view{"insert"}) {
      auto reId = handlePtr->addTempStoreItemValue(thread_id, text);
      return neograph::json{
          {"id", reId},
      }
          .dump();
    } else if (text_opt == std::string_view{"get"}) {
      if (text_id <= 0) {
        return R"({"error":"Arg `id` is empty"})";
      }
      auto result = handlePtr->getTempStoreItemValue(thread_id, text_id);
      return result.value_or(R"({"error":"Not found"})");
    } else if (text_opt == std::string_view{"set"}) {
      if (text_id <= 0) {
        return R"({"error":"Arg `id` is empty"})";
      }
      handlePtr->setTempStoreItemValue(thread_id, text_id, text);
      return "success";
    } else if (text_opt == std::string_view{"delete"}) {
      if (text_id <= 0) {
        return R"({"error":"Arg `id` is empty"})";
      }
      handlePtr->removeTempStoreItemValue(thread_id, text_id);
      return "success";
    } else {
      return R"({"error":"Arg `opt` is invalid"})";
    }
  }
};
} // namespace tools
} // namespace agentxx