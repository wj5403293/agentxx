#pragma once

#include "asio/random_access_file.hpp"
#include "asio/read.hpp"
#include "asio/read_at.hpp"
#include "asio/read_until.hpp"
#include "asio/registered_buffer.hpp"
#include "asio/stream_file.hpp"
#include "fmt/format.h"
#include "glob/glob.hpp"
#include "util/hyperscan.h"
#include "util/log.h"
#include "util/string_util.h"
#include <asio/as_tuple.hpp>
#include <asio/redirect_error.hpp>
#include <asio/strand.hpp>
#include <asio/write.hpp>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <iostream>
#include <limits>
#include <memory>
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
      result.push_back(neograph::json{
          {"path", entity.path().generic_string()},
          {"type", (entity.is_directory()      ? "dir"
                    : entity.is_regular_file() ? "file"
                    : entity.is_symlink()      ? "symlink"
                                               : "other")},
          {"size", size_t(entity.file_size())},
          {"last_write_time",
           entity.last_write_time().time_since_epoch().count()},
      });
    }
    co_return result.dump();
  }
};

/// read
class FilesystemReadTextFileTool : public neograph::AsyncTool {
protected:
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
                            {"description",
                             R"(文本偏移行数,默认`0`表示不偏移.如果偏移超出文件最大行数,将返回错误提示)"},
                        },
                    },
                    {
                        "line_limit",
                        {
                            {"type", "number"},
                            {"description",
                             R"(读取文本行数限制,取值范围 [1, ~],默认`null`表示不限制.允许指定的限制值超出文件最大行数不报错)"},
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

#if defined(ASIO_HAS_FILE)
    {
      auto currentIoCtx = co_await asio::this_coro::executor;

      /// 异步读取文件
      asio::stream_file stream{currentIoCtx};
      try {
        asio::error_code errCode;
        stream.open(filepath, asio::stream_file::read_only, errCode);
        if (false == stream.is_open()) {
          stream.close();
          throw std::runtime_error{
              fmt::format(R"(Can not open file: {}")", errCode.message())};
        }

        if (text_line_offset >= 0 || text_line_limit >= 0) {
          const auto offset =
              (text_line_offset >= 0) ? size_t(text_line_offset) : 0;
          const auto limit = (text_line_limit >= 0)
                                 ? size_t(text_line_limit)
                                 : std::numeric_limits<size_t>::max();
          std::stringstream result{};
          size_t lineNum = 0;

          for (std::string buf; lineNum < offset + limit; lineNum++) {
            auto readlen = co_await asio::async_read_until(
                stream, asio::dynamic_buffer(buf), '\n',
                asio::redirect_error(asio::use_awaitable, errCode));

            if (errCode == asio::error::eof) {
              if (lineNum >= offset) {
                result << buf;
              }
              break;
            } else if (errCode) {
              throw asio::system_error{errCode};
            }

            if (lineNum >= offset) {
              auto line = std::string_view{buf}.substr(0, readlen);
              result << line;
            }

            buf.erase(0, readlen);
          }

          stream.close();
          if (lineNum <= offset) {
            // offset 超出文件行数
            throw std::runtime_error{fmt::format(
                R"(Arg `line_offset`({} lines) is out of range of file lines({} lines).)",
                offset, lineNum)};
          }

          co_return result.str();
        }

        // 读取完整文件
        std::string data;
        co_await asio::async_read(
            stream, asio::dynamic_buffer(data), asio::transfer_all(),
            asio::redirect_error(asio::use_awaitable, errCode));
        if (errCode && errCode != asio::error::eof) {
          throw asio::system_error{errCode};
        }
        stream.close();
        co_return data;
      } catch (const std::exception &e) {
        stream.close();
        XX_LOGD("FilesystemReadTextFileTool exception: {}", e.what());
        throw e;
      }
    }
#endif

    {
      /// 同步阻塞读取文件
      std::ifstream stream;
      try {
        stream.open(filepath);
        if (!stream) {
          auto ec = std::error_code{errno, std::system_category()};
          throw std::runtime_error{
              fmt::format(R"(Can not open file. Error: {})", ec.message())};
        }

        if (text_line_offset >= 0 || text_line_limit >= 0) {
          // 读取部分文件
          const auto offset =
              (text_line_offset >= 0) ? size_t(text_line_offset) : 0;
          const auto limit = (text_line_limit > 0)
                                 ? size_t(text_line_limit)
                                 : std::numeric_limits<size_t>::max();
          std::stringstream result{};
          size_t lineNum = 0;

          for (std::string line;
               std::getline(stream, line) && lineNum < offset + limit;
               lineNum++) {
            // 跳过偏移行
            if (lineNum < offset) {
              continue;
            }

            result << line << "\n";
          }

          stream.close();
          if (lineNum <= offset) {
            // offset 超出文件行数
            throw std::runtime_error{fmt::format(
                R"(Arg `line_offset`({} lines) is out of range of file lines({} lines).)",
                offset, lineNum)};
          }

          co_return result.str();
        }

        // 读取完整文件
        auto result = std::string{std::istreambuf_iterator<char>(stream),
                                  std::istreambuf_iterator<char>()};
        stream.close();
        co_return result;
      } catch (const std::exception &e) {
        stream.close();
        XX_LOGD("FilesystemReadTextFileTool exception: {}", e.what());
        throw e;
      }
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
                             R"(起始读取字节偏移量,默认`0`表示不偏移.如果偏移超出文件大小,将返回错误提示)"},
                        },
                    },
                    {
                        "byte_limit",
                        {
                            {"type", "number"},
                            {"description",
                             R"(读取字节数限制,取值范围 [1, ~],默认`null`表示不限制.允许指定的限制值超出文件大小不报错)"},
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

#if defined(ASIO_HAS_FILE)
    {
      auto currentIoCtx = co_await asio::this_coro::executor;

      /// 异步读取文件
      if (byte_offset >= 0 || byte_limit >= 0) {
        asio::random_access_file stream{currentIoCtx};
        try {
          asio::error_code errCode;
          stream.open(filepath, asio::random_access_file::read_only, errCode);
          if (false == stream.is_open()) {
            stream.close();
            throw std::runtime_error{
                fmt::format(R"(Can not open file: {}")", errCode.message())};
          }

          // 读取部分文件
          const size_t offset = (byte_offset >= 0) ? size_t(byte_offset) : 0;
          const size_t limit = (byte_limit >= 0)
                                   ? size_t(byte_limit)
                                   : std::numeric_limits<size_t>::max();

          auto fileSize = stream.size();
          auto bytesAvailable =
              std::max((long long)fileSize - (long long)offset, (long long)0);
          auto bytesRead =
              std::min(static_cast<std::streamsize>(limit),
                       static_cast<std::streamsize>(bytesAvailable));

          // 没有数据可读
          if (bytesRead <= 0) {
            throw std::runtime_error{fmt::format(
                R"(Arg `byte_offset`({}) is out of range of file size({}).)",
                offset, (size_t)fileSize)};
          }

          std::string result;
          auto bytesReadLen = co_await asio::async_read_at(
              stream, byte_offset, asio::buffer(result, bytesRead),
              asio::redirect_error(asio::use_awaitable, errCode));
          if (errCode && errCode != asio::error::eof) {
            throw asio::system_error{errCode};
          }
          stream.close();
          auto readRange = std::string_view{result}.substr(0, bytesReadLen);
          co_return neograph::json{
              {"bytes_read_len", bytesReadLen},
              {
                  "base64_data",
                  agentxx::util::base64_encode(readRange.data(),
                                               readRange.size()),
              },
          }
              .dump();
        } catch (const std::exception &e) {
          stream.close();
          XX_LOGD("FilesystemReadBinaryFileTool exception: {}", e.what());
          throw e;
        }
      }

      // 读取完整文件
      asio::stream_file stream{currentIoCtx};
      try {
        asio::error_code errCode;
        stream.open(filepath, asio::stream_file::read_only, errCode);
        if (false == stream.is_open()) {
          stream.close();
          throw std::runtime_error{
              fmt::format(R"(Can not open file: {}")", errCode.message())};
        }

        std::string result;
        auto bytesReadLen = co_await asio::async_read(
            stream, asio::dynamic_buffer(result), asio::transfer_all(),
            asio::redirect_error(asio::use_awaitable, errCode));
        if (errCode && errCode != asio::error::eof) {
          throw asio::system_error{errCode};
        }
        stream.close();
        auto readRange = std::string_view{result}.substr(0, bytesReadLen);
        co_return neograph::json{
            {"bytes_read_len", bytesReadLen},
            {
                "base64_data",
                agentxx::util::base64_encode(readRange.data(),
                                             readRange.size()),
            },
        }
            .dump();
      } catch (const std::exception &e) {
        stream.close();
        XX_LOGD("FilesystemReadBinaryFileTool exception: {}", e.what());
        throw e;
      }
    }
#endif

    {
      /// 同步读取
      std::ifstream stream;
      try {
        stream.open(filepath, std::ios::binary);
        if (!stream) {
          auto ec = std::error_code{errno, std::system_category()};
          throw std::runtime_error{
              fmt::format(R"(Can not open file. Error: {})", ec.message())};
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
          auto bytesRead =
              std::min(static_cast<std::streamsize>(limit),
                       static_cast<std::streamsize>(bytesAvailable));

          // 没有数据可读
          if (bytesRead <= 0) {
            throw std::runtime_error{fmt::format(
                R"(Arg `byte_offset`({}) is out of range of file size({}).)",
                offset, (size_t)fileSize)};
          }

          stream.seekg(offset, std::ios::beg);
          if (!stream.good()) {
            auto ec = std::error_code{errno, std::system_category()};
            throw std::runtime_error{
                fmt::format(R"(Read offset {} bytes failed. Error: {})", offset,
                            ec.message())};
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
        std::streamsize bytesReadLen = stream.gcount();
        stream.close();
        co_return neograph::json{
            {"bytes_read_len", bytesReadLen},
            {"base64_data",
             agentxx::util::base64_encode(result.data(), result.size())},
        }
            .dump();
      } catch (const std::exception &e) {
        stream.close();
        XX_LOGD("FilesystemReadBinaryFileTool exception: {}", e.what());
        throw e;
      }
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
                    {
                        "is_binary",
                        {
                            {"type", "boolean"},
                            {"description",
                             R"(默认`false`,是否按二进制模式写入文件.
                             如果为`true`,参数`content`应当为base64编码的二进制数据.
                             如果为`false`,参数`content`视为普通文本,按字符串直接写入文件.)"},
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
    auto is_binary = arguments.value<bool>("is_binary", false);

#if defined(ASIO_HAS_FILE)
    {
      auto currentIoCtx = co_await asio::this_coro::executor;

      // 读取完整文件
      asio::stream_file stream{currentIoCtx};
      try {
        auto path = std::filesystem::path{filepath};
        if (false == overwrite && std::filesystem::exists(path)) {
          throw std::runtime_error{"File already exist"};
        }
        if (false == std::filesystem::exists(path.parent_path()) &&
            false == std::filesystem::create_directories(path.parent_path())) {
          // 创建父目录
          throw std::runtime_error{
              fmt::format(R"(Can not create `path`({})'s parent dirs.)",
                          path.parent_path().string())};
        }

        asio::error_code errCode;
        stream.open(filepath,
                    asio::stream_file::write_only | asio::stream_file::create,
                    errCode);
        if (false == stream.is_open()) {
          stream.close();
          throw std::runtime_error{
              fmt::format(R"(Can not open file: {}")", errCode.message())};
        }

        if (false == content.empty()) {
          // 写入文件内容
          if (is_binary) {
            auto result = agentxx::util::base64_decode(content);
            if (result.empty()) {
              throw std::runtime_error{"base64 decode failed"};
            }
            co_await asio::async_write(
                stream, asio::buffer(result),
                asio::redirect_error(asio::use_awaitable, errCode));
          } else {
            co_await asio::async_write(
                stream, asio::buffer(content),
                asio::redirect_error(asio::use_awaitable, errCode));
          }
          if (errCode) {
            throw asio::system_error{errCode};
          }
        }

        stream.close();
        co_return "success";
      } catch (const std::exception &e) {
        stream.close();
        XX_LOGD("FilesystemWriteFileTool exception: {}", e.what());
        throw e;
      }
    }
#endif

    {
      std::ofstream stream;
      try {
        auto path = std::filesystem::path{filepath};
        if (false == overwrite && std::filesystem::exists(path)) {
          throw std::runtime_error{"File already exist"};
        }
        if (false == std::filesystem::exists(path.parent_path()) &&
            false == std::filesystem::create_directories(path.parent_path())) {
          // 创建父目录
          throw std::runtime_error{
              fmt::format(R"(Can not create `path`({})'s parent dirs.)",
                          path.parent_path().string())};
        }

        stream.open(filepath, is_binary
                                  ? std::ios_base::out | std::ios_base::binary
                                  : std::ios_base::out);
        if (!stream) {
          auto ec = std::error_code{errno, std::system_category()};
          throw std::runtime_error{fmt::format(
              R"(Can not create or open file. Error: {})", ec.message())};
        }

        if (false == content.empty()) {
          // 写入文件内容
          if (is_binary) {
            auto result = agentxx::util::base64_decode(content);
            if (result.empty()) {
              throw std::runtime_error{"base64 decode failed"};
            }
            stream << result;
          } else {
            stream << content;
          }
          if (!stream) {
            auto ec = std::error_code{errno, std::system_category()};
            throw std::runtime_error{fmt::format(
                R"(File created success, but write failed. Error: {})",
                ec.message())};
          }
        }

        stream.close();
        co_return "success";
      } catch (const std::exception &e) {
        stream.close();
        XX_LOGD("FilesystemWriteFileTool exception: {}", e.what());
        throw e;
      }
    }
  }
};

/// edit file
class FilesystemEditTextFileTool : public neograph::AsyncTool {
public:
  explicit FilesystemEditTextFileTool() {}

  std::string get_name() const override { return "filesystem_edit_text_file"; }

  neograph::ChatTool get_definition() const override {
    return {
        "filesystem_edit_text_file",
        "Perform exact string replacements in text files(e.g. "
        "*.txt,*.md,*.cpp).",
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
                             R"(是否替换所有匹配`old_str`的字符串.默认`false`只替换第一个匹配)"},
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

#if defined(ASIO_HAS_FILE)
    {
      auto currentIoCtx = co_await asio::this_coro::executor;

      // 读取完整文件
      asio::stream_file stream{currentIoCtx};
      try {
        auto path = std::filesystem::path{filepath};
        if (false == std::filesystem::exists(path)) {
          throw std::runtime_error{"File not exist"};
        }

        asio::error_code errCode;
        stream.open(filepath, asio::stream_file::read_only, errCode);
        if (false == stream.is_open()) {
          stream.close();
          throw std::runtime_error{
              fmt::format(R"(Can not open file: {}")", errCode.message())};
        }

        std::string content;
        // 读出文件
        co_await asio::async_read(
            stream, asio::dynamic_buffer(content), asio::transfer_all(),
            asio::redirect_error(asio::use_awaitable, errCode));
        if (errCode && errCode != asio::error::eof) {
          throw asio::system_error{errCode};
        }
        stream.close();

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
          throw std::runtime_error{R"(No match `old_str` found)"};
        }

        // 覆盖写入文件内容
        stream.open(filepath, asio::stream_file::write_only, errCode);
        if (false == stream.is_open()) {
          stream.close();
          throw std::runtime_error{fmt::format(
              R"(Can not open file to write: {}")", errCode.message())};
        }
        co_await asio::async_write(
            stream, asio::buffer(content),
            asio::redirect_error(asio::use_awaitable, errCode));
        if (errCode) {
          throw asio::system_error{errCode};
        }

        stream.close();
        if (multi_replace) {
          co_return fmt::format(R"(Success, Replace {} times)", replaceHit);
        } else {
          co_return "success";
        }
      } catch (const std::exception &e) {
        stream.close();
        throw e;
      }
    }
#endif

    {
      std::fstream stream;
      try {
        auto path = std::filesystem::path{filepath};
        if (false == std::filesystem::exists(path)) {
          throw std::runtime_error{"File not exist"};
        }

        stream.open(filepath, std::ios_base::in);
        if (!stream) {
          auto ec = std::error_code{errno, std::system_category()};
          throw std::runtime_error{
              fmt::format(R"(Can not open file. Error: {})", ec.message())};
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
          throw std::runtime_error{R"(No match `old_str` found)"};
        }

        // 写入文件内容
        stream.close();
        stream.open(filepath, std::ios_base::out);
        if (!stream) {
          auto ec = std::error_code{errno, std::system_category()};
          throw std::runtime_error{fmt::format(
              R"(Can not open file to write. Error: {})", ec.message())};
        }
        stream << content;
        if (!stream) {
          auto ec = std::error_code{errno, std::system_category()};
          throw std::runtime_error{
              fmt::format(R"(Edit file failed. Error: {})", ec.message())};
        }

        stream.close();
        if (multi_replace) {
          co_return fmt::format(R"(Success, Replace {} times)", replaceHit);
        } else {
          co_return "success";
        }
      } catch (const std::exception &e) {
        stream.close();
        throw e;
      }
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
        R"(Find files matching patterns.)",
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
                             R"(Absolute dir or file path and glob pattern.

| Wildcard | Matches | Example
|--- |--- |--- |
| `*` | any characters | `*.txt` matches all files with the txt extension |
| `**` | any name dir recursively | `include/**/*.txt` matches all files with the txt extension in dir `include` and children dirs |
| `?` | any one character | `???` matches files with 3 characters long |
| `[]` | any character listed in the brackets | `[ABC]*` matches files starting with A,B or C | 
| `[-]` | any character in the range listed in brackets | `[A-Z]*` matches files starting with capital letters |
| `[!]` | any character not listed in the brackets | `[!ABC]*` matches files that do not start with A,B or C |

e.g., `/upload/**/*.txt`,`/docx/*[0-9].txt`,`/usr/include/nc*.h`,`/output/file[0-9].*`,`C:/down/read/??.txt`.
)"},
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

class FilesystemGrepTool : public neograph::AsyncTool {
public:
  explicit FilesystemGrepTool() {}

  std::string get_name() const override { return "filesystem_grep"; }

  neograph::ChatTool get_definition() const override {
    return {
        "filesystem_grep",
        R"(Searches file contents using regular expressions or text. 
Supports search of text content by string or full regex syntax through the `text_pattern` parameter. Filters files by pattern with the `file_pattern` parameter.
)",
        neograph::json{
            {"type", "object"},
            {
                "properties",
                {
                    {
                        "text_pattern_is_regex",
                        {
                            {"type", "boolean"},
                            {"description",
                             R"(The type of `text_pattern`.
`true`:  `text_pattern` is regex syntax.
`false`: `text_pattern` is crude text string.)"},
                        },
                    },
                    {
                        "text_pattern",
                        {
                            {"type", "string"},
                            {"description",
                             R"(String or regex syntax to search text content. The text match type depends on the `text_pattern_is_regex` parameter.)"},
                        },
                    },
                    {
                        "file_pattern",
                        {
                            {"type", "string"},
                            {"description",
                             R"(Absolute dir or file path and glob pattern.

| Wildcard | Matches | Example
|--- |--- |--- |
| `*` | any characters | `*.txt` matches all files with the txt extension |
| `**` | any name dir recursively | `include/**/*.txt` matches all files with the txt extension in dir `include` and children dirs |
| `?` | any one character | `???` matches files with 3 characters long |
| `[]` | any character listed in the brackets | `[ABC]*` matches files starting with A,B or C | 
| `[-]` | any character in the range listed in brackets | `[A-Z]*` matches files starting with capital letters |
| `[!]` | any character not listed in the brackets | `[!ABC]*` matches files that do not start with A,B or C |

e.g., `/upload/**/*.txt`,`/docx/*[0-9].txt`,`/usr/include/nc*.h`,`/output/file[0-9].*`,`C:/down/read/??.txt`.)"},
                        },
                    },
                    {
                        "output_mode",
                        {
                            {"type", "string"},
                            {"enum", neograph::json::array(
                                         {"files_with_matches", "content"})},
                            {"description",
                             R"(Default: `files_with_matches`. 
Output format:
'files_with_matches': Only file paths containing matches and count with `file:match_count` format
'content': Matching lines with file:line:content format)"},
                        },
                    },
                },
            },
            {"required", neograph::json::array({
                             "text_pattern_is_regex",
                             "text_pattern",
                             "file_pattern",
                         })},
        },
    };
  }

  asio::awaitable<std::string> readFileContent(const std::string &filepath) {
#if defined(ASIO_HAS_FILE)
    {
      auto currentIoCtx = co_await asio::this_coro::executor;

      /// 异步读取文件
      asio::stream_file stream{currentIoCtx};
      try {
        asio::error_code errCode;
        stream.open(filepath, asio::stream_file::read_only, errCode);
        if (false == stream.is_open()) {
          stream.close();
          throw std::runtime_error{fmt::format(
              R"(Can not open file. Error: {})", errCode.message())};
        }

        // 读取完整文件
        std::string data;
        co_await asio::async_read(
            stream, asio::dynamic_buffer(data), asio::transfer_all(),
            asio::redirect_error(asio::use_awaitable, errCode));
        if (errCode && errCode != asio::error::eof) {
          throw asio::system_error{errCode};
        }
        stream.close();
        co_return data;
      } catch (const std::exception &e) {
        stream.close();
        XX_LOGD("FilesystemGrepTool exception: {}", e.what());
        throw e;
      }
    }
#endif

    {
      /// 同步阻塞读取文件
      std::ifstream stream;
      try {
        stream.open(filepath);
        if (!stream) {
          auto ec = std::error_code{errno, std::system_category()};
          throw std::runtime_error{
              fmt::format(R"(Can not open file. Error: {})", ec.message())};
        }

        // 读取完整文件
        auto result = std::string{std::istreambuf_iterator<char>(stream),
                                  std::istreambuf_iterator<char>()};
        stream.close();
        co_return result;
      } catch (const std::exception &e) {
        stream.close();
        XX_LOGD("FilesystemGrepTool exception: {}", e.what());
        throw e;
      }
    }
  }

  asio::awaitable<std::string>
  execute_async(const neograph::json &arguments) override {
    auto text_pattern_is_regex = arguments.value("text_pattern_is_regex", true);
    auto text_pattern = arguments.value("text_pattern", std::string{});
    if (text_pattern.empty()) {
      co_return R"({"error":"Arg `text_pattern` is empty"})";
    }
    auto file_pattern = arguments.value("file_pattern", std::string{});
    if (file_pattern.empty()) {
      co_return R"({"error":"Arg `file_pattern` is empty"})";
    }
    auto output_mode =
        arguments.value("output_mode", std::string{"files_with_matches"});
    if (output_mode.empty()) {
      co_return R"({"error":"Arg `output_mode` is empty"})";
    }

    auto refilelist = glob::rglob(file_pattern);
    if (refilelist.empty()) {
      throw std::runtime_error{"No match `file_pattern` file found"};
    }

    bool isContainsMode = ("content" != output_mode);
    auto resultStr = std::ostringstream{};
    auto resultJson = neograph::json::array();

    if (text_pattern_is_regex) {
      // 正则匹配
      auto regex = agentxx::util::XXRegex{text_pattern};
      for (const auto &item : refilelist) {
        auto filepath = item.generic_string();
        auto filetext = co_await readFileContent(filepath);
        auto matchs = std::vector<agentxx::util::XXRegexMatchResult>{};
        if (regex.match(filetext, matchs)) {
          if (isContainsMode) {
            resultStr << filepath << ":" << matchs.size() << "\n";
          } else {
            auto matchsContent = neograph::json::array();
            size_t index = 0;
            size_t lineCount = 0;

            for (const auto &match : matchs) {
              // 计算到 match 时的行数
              for (size_t i = index; i < match.start; ++i) {
                if (filetext[i] == '\n') {
                  ++lineCount;
                }
              }

              matchsContent.push_back(neograph::json{
                  {"content",
                   std::string_view{filetext}.substr(match.start, match.end)},
                  {"line", lineCount},
              });
              index = match.start;
            }

            resultJson.push_back(neograph::json{
                {"filepath", filepath},
                {"matchs", matchsContent},
            });
          }
        }
      }
    } else {
      // 文本精确匹配
      auto search = agentxx::util::AhoCorasick{{text_pattern}, false};
      for (const auto &item : refilelist) {
        auto filepath = item.generic_string();
        auto filetext = co_await readFileContent(filepath);
        auto matchs = search.search(filetext);
        if (false == matchs.empty()) {
          if (isContainsMode) {
            resultStr << filepath << ":" << matchs.size() << "\n";
          } else {
            auto matchsContent = neograph::json::array();
            size_t index = 0;
            size_t lineCount = 0;

            for (const auto &match : matchs) {
              // 计算到 match 时的行数
              for (int i = (int)index; i < match.start; ++i) {
                if (filetext[i] == '\n') {
                  ++lineCount;
                }
              }

              matchsContent.push_back(neograph::json{
                  {"content", text_pattern},
                  {"line", lineCount},
              });
              index = match.start;
            }

            resultJson.push_back(neograph::json{
                {"filepath", filepath},
                {"matchs", matchsContent},
            });
          }
        }
      }
    }

    if (isContainsMode) {
      auto str = resultStr.str();
      if (false == str.empty()) {
        co_return str;
      } else {
        throw std::runtime_error{
            fmt::format("Found {} files match `file_pattern`, but no match "
                        "`text_pattern` file found.",
                        refilelist.size())};
      }
    } else {
      if (false == resultJson.empty()) {
        co_return resultJson.dump();
      } else {
        throw std::runtime_error{
            fmt::format("Found {} files match `file_pattern`, but no match "
                        "`text_pattern` file found.",
                        refilelist.size())};
      }
    }
  }
};

} // namespace tools
} // namespace agentxx