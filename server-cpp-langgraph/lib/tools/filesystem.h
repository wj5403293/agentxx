#pragma once

#include "util/log.h"
#include <cstdlib>
#include <filesystem>
#include <format>
#include <iostream>
#include <limits>
#include <memory>
#include <neograph/graph/deep_research_graph.h>
#include <neograph/llm/rate_limited_provider.h>
#include <neograph/llm/schema_provider.h>
#include <neograph/neograph.h>
#include <sstream>
#include <string>
#include <vector>

namespace agentxx {
namespace tools {

/// ls
class FileSystemListFileTool : public neograph::AsyncTool {
public:
  explicit FileSystemListFileTool() {}

  std::string get_name() const override { return "filesystem_listfile"; }

  neograph::ChatTool get_definition() const override {
    return {
        "filesystem_listfile",
        R"(列出文件夹内的文件和文件夹信息，包含文件大小/Bytes, 类型, 最后写入时间(时间戳/nanoseconds)
指定文件路径可以得到文件信息.
可用于检查文件/文件夹是否存在.)",
        neograph::json{
            {"type", "object"},
            {
                "properties",
                {{
                    "path",
                    {
                        {"type", "string"},
                        {"description", "文件或文件夹的绝对路径."},
                    },
                }},
            },
            {"required", neograph::json::array({"path"})},
        },
    };
  }

  asio::awaitable<std::string>
  execute_async(const neograph::json &arguments) override {
    auto targetPath = arguments.value("path", std::string{});
    if (targetPath.empty()) {
      co_return R"({"error":"Arg `path` is empty"})";
    }
    std::cout << (targetPath) << std::endl;
    try {
      auto result = neograph::json::array();
      for (const auto &entity :
           std::filesystem::directory_iterator(targetPath)) {
        auto item = neograph::json{
            {"path", entity.path().generic_string()},
            {"type", (entity.is_directory()      ? "dir"
                      : entity.is_regular_file() ? "file"
                      : entity.is_symlink()      ? "symlink"
                                                 : "other")},
            {"size", size_t(entity.file_size())},
            {"last_write_time",
             entity.last_write_time().time_since_epoch().count()},
        };
        result.push_back(item);
      }
      co_return result.dump();
    } catch (const std::exception &e) {
      co_return std::format(R"({{"error": "filesystem_listfile failed: {}"}})",
                            e.what());
    }
  }
};

/// read
class FilesystemReadFile : public neograph::AsyncTool {
public:
  explicit FilesystemReadFile() {}

  std::string get_name() const override { return "filesystem_readfile"; }

  neograph::ChatTool get_definition() const override {
    return {
        "filesystem_readfile",
        "Read file contents with line numbers, supports offset/limit for large "
        "files."
        "Also supports returning multimodal content blocks for non-text files "
        "(images, video, audio, and documents).",
        neograph::json{
            {"type", "object"},
            {
                "properties",
                {
                    {
                        "path",
                        {
                            {"type", "string"},
                            {"description", "Absolute file path."},
                        },
                    },
                    {
                        "text_line_offset",
                        {
                            {"type", "number"},
                            {"description", "文本偏移行数，默认 0 "
                                            "表示不偏移。如果偏移超出文件最大行"
                                            "数，将返回错误提示"},
                        },
                    },
                    {
                        "text_line_limit",
                        {
                            {"type", "number"},
                            {"description",
                             "读取文本行数限制，取值范围 [1, ~]，默认 null "
                             "表示不限制。允许指定的限制值超出文件最大行数不报"
                             "错"},
                        },
                    },
                },
            },
            {"required", neograph::json::array({"path"})},
        },
    };
  }

  asio::awaitable<std::string>
  execute_async(const neograph::json &arguments) override {
    auto filepath = arguments.value("path", std::string{});
    if (filepath.empty()) {
      co_return R"({"error":"Arg `path` is empty"})";
    }
    auto text_line_offset = arguments.value<double>("text_line_offset", -1);
    auto text_line_limit = arguments.value<double>("text_line_limit", -1);

    try {
      auto stream = std::ifstream{filepath};
      if (text_line_offset >= 0 || text_line_limit >= 0) {
        // 读取部分文件
        const auto offset =
            (text_line_offset >= 0) ? size_t(text_line_offset) : 0;
        const auto limit = (text_line_limit >= 0)
                               ? size_t(text_line_limit)
                               : std::numeric_limits<size_t>::max();
        std::stringstream result{};
        int line_num = 0;
        int count = 0;

        for (std::string line; std::getline(stream, line) && count < limit;) {
          // 跳过偏移行
          if (line_num < offset) {
            line_num++;
            continue;
          }
          // 达到限制则停止
          if (limit != -1 && count >= limit) {
            break;
          }
          result << line << "\n";

          line_num++;
          count++;
        }
        co_return result.str();
      }
      // 读取完整文件
      co_return std::string((std::istreambuf_iterator<char>(stream)),
                            std::istreambuf_iterator<char>());
    } catch (const std::exception &e) {
      co_return std::format(R"({{"error": "filesystem_readfile failed: {}"}})",
                            e.what());
    }
  }
};

} // namespace tools
} // namespace agentxx