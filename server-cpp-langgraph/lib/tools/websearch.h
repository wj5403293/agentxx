#pragma once

#include <neograph/llm/rate_limited_provider.h>
#include <neograph/llm/schema_provider.h>
#include <neograph/neograph.h>

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib.h>

#include "util/http_client.h"
#include "util/log.h"
#include <cstdlib>
#include <format>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace agentxx {
namespace tools {

class WebSearchTool : public neograph::AsyncTool {
public:
  explicit WebSearchTool() {}

  std::string get_name() const override { return "web_search"; }

  neograph::ChatTool get_definition() const override {
    return {
        "web_search",
        "query 进行网络搜索. 返回一个 markdown 列表结果. "
        "然后可以使用 fetch_url_markdown 工具拉取网页具体内容.",
        neograph::json{
            {"type", "object"},
            {
                "properties",
                {{
                    "query",
                    {
                        {"type", "string"},
                        {"description", "Search query."},
                    },
                }},
            },
            {"required", neograph::json::array({"query"})},
        },
    };
  }

  asio::awaitable<std::string>
  execute_async(const neograph::json &arguments) override {
    std::string query = arguments.value("query", std::string{});
    if (query.empty()) {
      co_return R"({"error":"Arg `query` is empty"})";
    }
    // TODO: 更换api
    auto search_url =
        std::format("https://www.baidu.com/s?wd={}",
                    agentxx::util::HttpClient_c::urlEncode(query));

    auto [resp, resp_err] =
        co_await agentxx::util::HttpClient_c::fetchMarkdown(search_url);
    if (resp.has_value()) {
      auto &data = resp.value();
      if (data.empty()) {
        co_return R"({"error": "Empty search result."})";
      }
      const auto maxLength = 8000;
      if (data.size() > maxLength) {
        data.resize(maxLength);
      }
      co_return data;
    }
    co_return std::format(
        R"({{"error":"web_search failed: {}"}})",
        resp_err.value_or(std::runtime_error("[unknown]")).what());
  }
};

class FetchUrlTool : public neograph::AsyncTool {
public:
  explicit FetchUrlTool() {}

  std::string get_name() const override { return "fetch_url"; }

  neograph::ChatTool get_definition() const override {
    return {
        "fetch_url",
        "(Http GET) 发送GET请求，返回响应体.",
        neograph::json{
            {"type", "object"},
            {
                "properties",
                {{
                     "url",
                     {
                         {"type", "string"},
                         {"description", "Absolute http/https URL."},
                     },
                 },
                 {
                     "timeout",
                     {
                         {"type", "number"},
                         {"description",
                          "GET requiest timeout, default 60 seconds."},
                     },
                 }},
            },
            {"required", neograph::json::array({"url"})},
        },
    };
  }

  asio::awaitable<std::string>
  execute_async(const neograph::json &arguments) override {
    auto url = arguments.value("url", std::string{});
    if (url.empty()) {
      co_return R"({"error":"Arg `url` is empty"})";
    }
    int timeout = int(arguments.value<double>("timeout", 60.0));

    auto [resp, resp_err] = co_await agentxx::util::HttpClient_c::getAsync(
        url, std::chrono::seconds(timeout));
    if (resp.has_value()) {
      if (false == agentxx::util::HttpClient_c::respIsSucc(resp.value())) {
        co_return std::format(
            R"({{"error":"fetch_url failed, status {}, error: {}"}})",
            resp.value().status,
            resp_err.value_or(std::runtime_error("[none]")).what());
      }

      auto &data = resp.value().body;
      if (data.empty()) {
        co_return R"({"error": "Http GET request Success, but got empty body."})";
      }
      co_return data;
    }
    co_return std::format(
        R"({{"error":"fetch_url failed: {}"}})",
        resp_err.value_or(std::runtime_error("[unknown]")).what());
  }
};

class FetchUrlMarkdownTool : public neograph::AsyncTool {
public:
  explicit FetchUrlMarkdownTool() {}

  std::string get_name() const override { return "fetch_url_markdown"; }

  neograph::ChatTool get_definition() const override {
    return {
        "fetch_url_markdown",
        R"((Http GET) 拉取一个网页,返回其Markdown格式的页面内容. 
常用于在 web_search 之后获取具体页面内容.
如果需要获取MD中的相对路径链接网页,应当结合本次传入的`url`. 例如:
- 网页`http://example.com/help/`内:
    - 包含相对路径`model/delete/`(非/开头为相对路径),则顺着当前网页末尾拼接得到的完整链接为`http://example.com/help/model/delete/`
    - 包含相对路径`./model/create/`(以.开头为相对路径),则顺着当前网页末尾拼接得到的完整链接为`http://example.com/help/model/create/`
    - 包含相对路径`../model/create/`(以..开头为相对路径，上一级目录),则顺着当前网页末尾拼接得到的完整链接为`http://example.com/model/create/`
    - 包含绝对路径`/model/view/`(以/开头为绝对路径),则替换网页路径得到`http://example.com/model/view/`
- 网页`http://example.com/help/what.html`内:
    - 包含相对路径`model/delete/`(非/开头为相对路径),则移除末尾文件名，顺着当前网页末尾拼接，得到的完整链接为`http://example.com/help/model/delete/`
)",
        neograph::json{
            {"type", "object"},
            {
                "properties",
                {{
                    "url",
                    {
                        {"type", "string"},
                        {"description", "Absolute http/https URL."},
                    },
                }},
            },
            {"required", neograph::json::array({"url"})},
        },
    };
  }

  asio::awaitable<std::string>
  execute_async(const neograph::json &arguments) override {
    std::string url = arguments.value("url", std::string{});
    if (url.empty()) {
      co_return R"({"error":"Arg `url` is empty"})";
    }

    auto [resp, resp_err] =
        co_await agentxx::util::HttpClient_c::fetchMarkdown(url);
    if (resp.has_value()) {
      auto &data = resp.value();
      if (data.empty()) {
        co_return R"({"error": "Request Success, but got empty result."})";
      }
      const auto maxLength = 5000;
      if (data.size() > maxLength) {
        data.resize(maxLength);
      }
      co_return data;
    }
    co_return std::format(
        R"({{"error":"fetch_url_markdown failed: {}"}})",
        resp_err.value_or(std::runtime_error("[unknown]")).what());
  }
};
} // namespace tools
} // namespace agentxx