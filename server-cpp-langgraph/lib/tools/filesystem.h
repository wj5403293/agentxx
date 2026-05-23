#pragma once

#include "glob/glob.hpp"
#include "util/log.h"
#include "util/string_util.h"
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
                        {"description", "文件或文件夹的绝对路径"},
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

    auto result = neograph::json::array();
    for (const auto &entity : std::filesystem::directory_iterator(targetPath)) {
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
  }
};

/// read
class FilesystemReadTextFileTool : public neograph::AsyncTool {
public:
  explicit FilesystemReadTextFileTool() {}

  std::string get_name() const override { return "filesystem_read_text_file"; }

  neograph::ChatTool get_definition() const override {
    return {
        "filesystem_read_text_file",
        "Read text file (e.g.: .txt,.md,.json,.log) contents with line "
        "numbers, supports offset/limit for large "
        "files.",
        neograph::json{
            {"type", "object"},
            {
                "properties",
                {
                    {
                        "path",
                        {
                            {"type", "string"},
                            {"description", "文件绝对路径"},
                        },
                    },
                    {
                        "line_offset",
                        {
                            {"type", "number"},
                            {"description", "文本偏移行数,默认`0`"
                                            "表示不偏移.如果偏移超出文件最大行"
                                            "数,将返回错误提示"},
                        },
                    },
                    {
                        "line_limit",
                        {
                            {"type", "number"},
                            {"description",
                             "读取文本行数限制,取值范围 [1, ~],默认`null`"
                             "表示不限制.允许指定的限制值超出文件最大行数不报"
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
    auto text_line_offset = arguments.value<double>("line_offset", -1);
    auto text_line_limit = arguments.value<double>("line_limit", -1);

    std::ifstream stream;
    try {
      stream.open(filepath);
      if (!stream) {
        auto ec = std::error_code{errno, std::system_category()};
        stream.close();
        co_return std::format(R"({{"error":"Can not open file. Error: {}"}})",
                              ec.message());
      }

      if (text_line_offset >= 0 || text_line_limit >= 0) {
        // 读取部分文件
        const auto offset =
            (text_line_offset >= 0) ? size_t(text_line_offset) : 0;
        const auto limit = (text_line_limit >= 0)
                               ? size_t(text_line_limit)
                               : std::numeric_limits<size_t>::max();
        std::stringstream result{};
        size_t line_num = 0;
        size_t count = 0;

        for (std::string line; std::getline(stream, line) && count < limit;) {
          // 跳过偏移行
          if (line_num < offset) {
            line_num++;
            continue;
          }
          // 达到限制则停止
          if (count >= limit) {
            break;
          }
          result << line << "\n";

          line_num++;
          count++;
        }

        stream.close();
        if (0 == count) {
          // offset 超出文件行数
          co_return std::format(
              R"({{"error":"Arg `line_offset`({} lines) is out of range of file lines({} lines)."}})",
              offset, line_num);
        }
        co_return result.str();
      }
      // 读取完整文件
      auto result = std::string((std::istreambuf_iterator<char>(stream)),
                                std::istreambuf_iterator<char>());
      stream.close();
      co_return result;
    } catch (const std::exception &e) {
      stream.close();
      throw e;
    }
  }
};

/// read
class FilesystemReadBinaryFileTool : public neograph::AsyncTool {
public:
  explicit FilesystemReadBinaryFileTool() {}

  std::string get_name() const override {
    return "filesystem_read_binary_file";
  }

  neograph::ChatTool get_definition() const override {
    return {
        "filesystem_read_binary_file",
        "Read binary file (e.g.: .txt,.md,.json,.log) contents with byte "
        "offset. Supports offset/limit for large files. Returns binary content "
        "as base64 string.",
        neograph::json{
            {"type", "object"},
            {
                "properties",
                {
                    {
                        "path",
                        {
                            {"type", "string"},
                            {"description", "文件绝对路径"},
                        },
                    },
                    {
                        "byte_offset",
                        {
                            {"type", "number"},
                            {"description",
                             "起始读取字节偏移量,默认`0`表示不偏移."
                             "如果偏移超出文件大小,将返回错误提示"},
                        },
                    },
                    {
                        "byte_limit",
                        {
                            {"type", "number"},
                            {"description",
                             "读取字节数限制,取值范围 [1, ~],默认`null`"
                             "表示不限制.允许指定的限制值超出文件大小不报错"},
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
    auto byte_offset = arguments.value<double>("byte_offset", -1);
    auto byte_limit = arguments.value<double>("byte_limit", -1);

    std::ifstream stream;
    try {
      stream.open(filepath, std::ios::binary);
      if (!stream) {
        auto ec = std::error_code{errno, std::system_category()};
        stream.close();
        co_return std::format(R"({{"error":"Can not open file. Error: {}"}})",
                              ec.message());
      }

      if (byte_offset >= 0 || byte_limit >= 0) {
        // 读取部分文件
        const size_t offset = (byte_offset >= 0) ? size_t(byte_offset) : 0;
        const size_t limit = (byte_limit >= 0)
                                 ? size_t(byte_limit)
                                 : std::numeric_limits<size_t>::max();

        // 计算实际需要读取的字节数
        auto fileSize = stream.tellg();
        auto bytesAvailable =
            std::max((long long)fileSize - (long long)offset, (long long)0);
        auto bytesRead = std::min(static_cast<std::streamsize>(limit),
                                  static_cast<std::streamsize>(bytesAvailable));

        // 没有数据可读
        if (bytesRead <= 0) {
          stream.close();
          co_return std::format(
              R"({{"error":"Arg `byte_offset`({}) is out of range of file size({})."}})",
              offset, (size_t)fileSize);
        }

        stream.seekg(offset, std::ios::beg);
        if (!stream.good()) {
          auto ec = std::error_code{errno, std::system_category()};
          stream.close();
          co_return std::format(
              R"({{"error":"Read offset {} bytes failed. Error: {}"}})", offset,
              ec.message());
        }

        std::vector<char> result{};
        result.resize(bytesRead + 1);
        stream.read(result.data(), limit);
        std::streamsize realBytesRead = stream.gcount();

        stream.close();
        co_return neograph::json{
            {"bytes_read_len", realBytesRead},
            {"base64_data",
             agentxx::util::base64_encode(result.data(), result.size())},
        }
            .dump();
      }

      // 读取完整文件
      auto result = std::string((std::istreambuf_iterator<char>(stream)),
                                std::istreambuf_iterator<char>());
      std::streamsize bytes_read = stream.gcount();
      stream.close();
      co_return neograph::json{
          {"bytes_read_len", bytes_read},
          {"base64_data",
           agentxx::util::base64_encode(result.data(), result.size())},
      }
          .dump();
    } catch (const std::exception &e) {
      stream.close();
      throw e;
    }
  }
};

/// write
class FilesystemWriteFileTool : public neograph::AsyncTool {
public:
  explicit FilesystemWriteFileTool() {}

  std::string get_name() const override { return "filesystem_writefile"; }

  neograph::ChatTool get_definition() const override {
    return {
        "filesystem_writefile",
        "创建新文件，或覆盖文件.",
        neograph::json{
            {"type", "object"},
            {
                "properties",
                {
                    {
                        "path",
                        {
                            {"type", "string"},
                            {"description", "文件绝对路径"},
                        },
                    },
                    {
                        "content",
                        {
                            {"type", "string"},
                            {"description", "写入文件的内容"},
                        },
                    },
                    {
                        "overwrite",
                        {
                            {"type", "boolean"},
                            {"description",
                             R"(默认`false`,是否覆盖文件.
                             如果为`true`,仅创建新文件并写入,若文件已存在则返回失败.
                             如果为`false`,若文件不存在则创建并写入,若文件已经存在,则覆盖文件内容.)"},
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
    auto content = arguments.value<std::string>("content", std::string{});
    auto overwrite = arguments.value<bool>("overwrite", false);

    std::ofstream stream;
    try {
      auto path = std::filesystem::path{filepath};
      if (false == overwrite && std::filesystem::exists(path)) {
        co_return R"({"error":"File already exist."})";
      }
      if (false == std::filesystem::exists(path.parent_path()) &&
          false == std::filesystem::create_directories(path.parent_path())) {
        // 创建父目录
        co_return std::format(
            R"({{"error":"Can not create `path`({})'s parent dirs."}})",
            path.parent_path().string());
      }

      stream.open(filepath, std::ios_base::out);
      if (!stream) {
        auto ec = std::error_code{errno, std::system_category()};
        stream.close();
        co_return std::format(
            R"({{"error":"Can not create or open file. Error: {}"}})",
            ec.message());
      }

      if (false == content.empty()) {
        // 写入文件内容
        stream << content;
        if (!stream) {
          auto ec = std::error_code{errno, std::system_category()};
          stream.close();
          co_return std::format(
              R"({{"error":"File created success, but write failed. Error: {}"}})",
              ec.message());
        }
      }

      stream.close();
      co_return "success";
    } catch (const std::exception &e) {
      stream.close();
      throw e;
    }
  }
};

/// edit file
class FilesystemEditFileTool : public neograph::AsyncTool {
public:
  explicit FilesystemEditFileTool() {}

  std::string get_name() const override { return "filesystem_editfile"; }

  neograph::ChatTool get_definition() const override {
    return {
        "filesystem_editfile",
        "Perform exact string replacements in files (with global replace "
        "mode).",
        neograph::json{
            {"type", "object"},
            {
                "properties",
                {
                    {
                        "path",
                        {
                            {"type", "string"},
                            {"description", "文件绝对路径"},
                        },
                    },
                    {
                        "old_str",
                        {
                            {"type", "string"},
                            {"description",
                             "待替换的旧字符串,精准匹配,不能为空"},
                        },
                    },
                    {
                        "new_str",
                        {
                            {"type", "string"},
                            {"description", "新字符串"},
                        },
                    },
                    {
                        "multi_replace",
                        {
                            {"type", "boolean"},
                            {"description",
                             "是否替换所有匹配`old_str`"
                             "的字符串.默认`false`只替换第一个匹配"},
                        },
                    },

                },
            },
            {"required", neograph::json::array({"path", "old_str", "new_str"})},
        },
    };
  }

  asio::awaitable<std::string>
  execute_async(const neograph::json &arguments) override {
    auto filepath = arguments.value("path", std::string{});
    if (filepath.empty()) {
      co_return R"({"error":"Arg `path` is empty"})";
    }
    auto old_str = arguments.value<std::string>("old_str", std::string{});
    if (old_str.empty()) {
      co_return R"({"error":"Arg `old_str` is empty"})";
    }
    auto new_str = arguments.value<std::string>("new_str", std::string{});
    auto multi_replace = arguments.value<bool>("multi_replace", false);

    std::fstream stream;
    try {
      auto path = std::filesystem::path{filepath};
      if (false == std::filesystem::exists(path)) {
        co_return R"({"error":"File not exist."})";
      }

      stream.open(filepath, std::ios_base::in | std::ios_base::out |
                                std::ios_base::binary);
      if (!stream) {
        auto ec = std::error_code{errno, std::system_category()};
        stream.close();
        co_return std::format(R"({{"error":"Can not open file. Error: {}"}})",
                              ec.message());
      }

      std::ostringstream output;
      output << stream.rdbuf();
      std::string content = output.str();

      int replaceHit = 0;
      size_t pos = 0;
      while ((pos = content.find(old_str, pos)) != std::string::npos) {
        replaceHit++;
        content.replace(pos, old_str.length(), new_str);
        // 跳过新字符串，避免死循环
        pos += new_str.length();
        if (false == multi_replace) {
          break;
        }
      }

      if (0 == replaceHit) {
        stream.close();
        co_return R"({"error":"No match `old_str` found"})";
      }

      // 写入文件内容
      stream.seekp(0);
      stream << content;
      if (!stream) {
        auto ec = std::error_code{errno, std::system_category()};
        stream.close();
        co_return std::format(R"({{"error":"Edit file failed. Error: {}"}})",
                              ec.message());
      }

      stream.close();
      if (multi_replace) {
        co_return std::format(R"(Success, Replace {} times)", replaceHit);
      } else {
        co_return "success";
      }
    } catch (const std::exception &e) {
      stream.close();
      throw e;
    }
  }
};

class FilesystemGlobTool : public neograph::AsyncTool {
public:
  explicit FilesystemGlobTool() {}

  std::string get_name() const override { return "filesystem_glob"; }

  neograph::ChatTool get_definition() const override {
    return {
        "filesystem_glob",
        R"(Find files matching patterns.

| Wildcard | Matches | Example
|--- |--- |--- |
| `*` | any characters | `*.txt` matches all files with the txt extension |
| `**` | any name dir recursively | `include/**/*.txt` matches all files with the txt extension in dir `include` and children dirs |
| `?` | any one character | `???` matches files with 3 characters long |
| `[]` | any character listed in the brackets | `[ABC]*` matches files starting with A,B or C | 
| `[-]` | any character in the range listed in brackets | `[A-Z]*` matches files starting with capital letters |
| `[!]` | any character not listed in the brackets | `[!ABC]*` matches files that do not start with A,B or C |

e.g., `/upload/**/*.txt`,`/docx/*[0-9].txt`,`/usr/include/nc*.h`,`/output/file[0-9].*`,`C:/down/read/??.txt`.
)",
        neograph::json{
            {"type", "object"},
            {
                "properties",
                {
                    {
                        "pattern",
                        {
                            {"type", "string"},
                            {"description",
                             "Absolute dir path and glob pattern"},
                        },
                    },
                },
            },
            {"required", neograph::json::array({"pattern"})},
        },
    };
  }

  asio::awaitable<std::string>
  execute_async(const neograph::json &arguments) override {
    auto searchPattern = arguments.value("pattern", std::string{});
    if (searchPattern.empty()) {
      co_return R"({"error":"Arg `pattern` is empty"})";
    }

    auto relist = glob::rglob(searchPattern);
    if (relist.empty()) {
      co_return R"({"error":"No match `pattern` found"})";
    }

    auto result = std::ostringstream{};
    for (auto &item : relist) {
      result << item.string() << std::endl;
    }

    co_return result.str();
  }
};

} // namespace tools
} // namespace agentxx