#pragma once

#include "agentxx/tools/tool.h"
#include "agentxx/util/http_client.h"
#include "agentxx/util/log.h"
#include "fmt/format.h"
#include <cstdlib>
#include <iostream>
#include <memory>
#include <neograph/llm/rate_limited_provider.h>
#include <neograph/llm/schema_provider.h>
#include <neograph/neograph.h>
#include <sstream>
#include <string>
#include <vector>

namespace agentxx {
namespace tools {

class WebSearchTool : public XXToolBase {
protected:
  const std::string searchApiUrl;
  const bool convertHtml2markdown;

public:
  explicit WebSearchTool(std::string_view in_searchApiUrl,
                         bool in_convertHtml2markdown)
      : XXToolBase("web_search", true, true), searchApiUrl(in_searchApiUrl),
        convertHtml2markdown(in_convertHtml2markdown) {}

  neograph::ChatTool get_definition() const override {
    return {
        "web_search",
        "进行网络搜索. 返回一个 markdown 列表结果. "
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
    auto search_url = fmt::format(fmt::runtime(searchApiUrl),
                                  agentxx::util::HttpClient::urlEncode(query));

    std::optional<std::string> out_resp_err;
    if (convertHtml2markdown) {
      auto resp = co_await agentxx::util::HttpClient::getAsync(
          search_url, std::chrono::seconds{15});
      out_resp_err = resp.error_or("unknown");
      if (resp.has_value()) {
        auto &respVal = resp.value();
        if (agentxx::util::HttpClient::respIsSucc(respVal)) {
          auto &data = respVal.body;
          if (data.empty()) {
            co_return R"({"error": "Empty search result."})";
          }
          const auto maxLength = 8000;
          if (data.size() > maxLength) {
            data.resize(maxLength);
          }
          co_return data;
        }
      }
    } else {
      auto resp = co_await agentxx::util::HttpClient::fetchMarkdown(search_url);
      out_resp_err = resp.error_or("unknown");
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
    }
    throw std::runtime_error(out_resp_err.value_or("[unknown]"));
  }
};

class WebFetchUrlTool : public XXToolBase {
public:
  explicit WebFetchUrlTool() : XXToolBase("web_fetch_url", true, true) {}

  neograph::ChatTool get_definition() const override {
    return {
        "web_fetch_url",
        "(Http GET) 发起网络请求,返回响应体原文.",
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

    auto resp = co_await agentxx::util::HttpClient::getAsync(
        url, std::chrono::seconds(timeout));
    if (resp.has_value()) {
      if (false == agentxx::util::HttpClient::respIsSucc(resp.value())) {
        co_return fmt::format(
            R"({{"error":"web_fetch_url failed, status {}, error: {}"}})",
            resp.value().status, resp.error_or("[unknown]"));
      }

      auto &data = resp.value().body;
      if (data.empty()) {
        co_return R"({"error": "Http GET request Success, but got empty body."})";
      }
      std::string encoding;
      std::string converted;
      if (agentxx::util::autoConvertToUtf8(data, encoding, converted)) {
        co_return converted;
      }
      co_return data;
    }
    throw std::runtime_error(resp.error_or("[unknown]"));
  }
};

class WebFetchUrlMarkdownTool : public XXToolBase {
public:
  explicit WebFetchUrlMarkdownTool()
      : XXToolBase("web_fetch_url_markdown", true, true) {}

  neograph::ChatTool get_definition() const override {
    return {
        "web_fetch_url_markdown",
        R"((Http GET) 拉取一个网页,返回其Markdown格式的页面内容. 
常用于在 web_search 之后获取具体页面内容.)",
        neograph::json{
            {"type", "object"},
            {
                "properties",
                {{
                    "url",
                    {
                        {"type", "string"},
                        {"description", R"(Absolute http/https URL.
如果需要获取MD中的相对路径链接网页,应当结合本次传入的`url`. 例如:
- 网页`http://example.com/help/`内:
    - 包含相对路径`model/delete/`(非/开头为相对路径),则顺着当前网页末尾拼接得到的完整链接为`http://example.com/help/model/delete/`
    - 包含相对路径`./model/create/`(以.开头为相对路径),则顺着当前网页末尾拼接得到的完整链接为`http://example.com/help/model/create/`
    - 包含相对路径`../model/create/`(以..开头为相对路径，上一级目录),则顺着当前网页末尾拼接得到的完整链接为`http://example.com/model/create/`
    - 包含绝对路径`/model/view/`(以/开头为绝对路径),则替换网页路径得到`http://example.com/model/view/`
- 网页`http://example.com/help/what.html`内:
    - 包含相对路径`model/delete/`(非/开头为相对路径),则移除末尾文件名，顺着当前网页末尾拼接，得到的完整链接为`http://example.com/help/model/delete/`
)"},
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

    auto resp = co_await agentxx::util::HttpClient::fetchMarkdown(url);
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
    throw std::runtime_error(resp.error_or("[unknown]"));
  }
};
} // namespace tools
} // namespace agentxx