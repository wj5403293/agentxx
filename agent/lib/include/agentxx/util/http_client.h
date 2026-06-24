#pragma once

#include "agentxx/util/log.h"
#include "html2md/html2md.h"
#include <asio/awaitable.hpp>
#include <asio/connect.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/this_coro.hpp>
#include <expected>
#include <format>
#include <httplib.h>
#include <neograph/async/endpoint.h>
#include <neograph/async/http_client.h>
#include <neograph/json.h>
#include <optional>

namespace agentxx {
namespace util {

class HttpClient {
public:
  static inline std::pair<std::string, std::string>
  splitUrl(const std::string &url) {
    auto scheme_end = url.find("://");
    if (scheme_end == std::string::npos) {
      return {url, "/"};
    }
    auto path_start = url.find('/', scheme_end + 3);
    if (path_start == std::string::npos) {
      return {url, "/"};
    }
    return {url.substr(0, path_start), url.substr(path_start)};
  }

  // URL-encode for query strings (small subset: letters/digits untouched,
  // space→+, everything else percent-encoded).
  static inline std::string urlEncode(const std::string &s) {
    std::ostringstream out;
    out << std::hex;
    for (unsigned char c : s) {
      if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
          c == '~') {
        out << c;
      } else if (c == ' ') {
        out << '+';
      } else {
        out << '%';
        if (c < 16)
          out << '0';
        out << (int)c;
        out << std::dec << std::hex; // sticky std::hex
      }
    }
    return out.str();
  }

  static inline bool respIsSucc(const neograph::async::HttpResponse &resp) {
    return (resp.status / 100 == 2);
  }

  static inline bool respIsSucc(const httplib::Result &resp) {
    return (resp && resp->status / 100 == 2);
  }

  static inline asio::awaitable<
      std::expected<neograph::async::HttpResponse, std::string>>
  getAsync(const std::string &url,
           std::chrono::milliseconds timeout = std::chrono::seconds{60}) {
    auto endpoint = neograph::async::split_async_endpoint(url);
    if (endpoint.prefix.empty()) {
      endpoint.prefix = "/";
    }

    auto headers = std::vector<std::pair<std::string, std::string>>{
        {"User-Agent",
         R"(Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/119.0.6045.160 Safari/537.36)"},
        {"accept-language", "zh-CN,zh;q=0.9"},
    };
    neograph::async::RequestOptions opts;
    opts.timeout = timeout;
    opts.max_redirects = 5;

    auto ex = co_await asio::this_coro::executor;

    try {
      // TODO: 302 会失败
      auto resp = co_await neograph::async::async_get(
          ex, endpoint.host, endpoint.port, endpoint.prefix, std::move(headers),
          endpoint.tls, opts);
      co_return std::expected<neograph::async::HttpResponse, std::string>{
          std::move(resp)};
    } catch (const std::exception &e) {
      XX_LOGE("http_get failed ({}): {}", url, e.what());
      co_return std::unexpected{e.what()};
    }
  }

  static inline asio::awaitable<std::expected<::httplib::Result, std::string>>
  postAsync(const std::string &url, const neograph::json &body,
            std::chrono::milliseconds timeout = std::chrono::seconds{60}) {
    auto endpoint = neograph::async::split_async_endpoint(url);
    if (endpoint.prefix.empty()) {
      endpoint.prefix = "/";
    }

    try {
      auto cli = httplib::Client{fmt::format(
          "{}://{}:{}", endpoint.tls ? "https" : "http", endpoint.host,
          (endpoint.port.empty() ? (endpoint.tls ? "443" : "80")
                                 : endpoint.port))};
      cli.set_read_timeout(30, 0);

      httplib::Headers headers = {
          {"accept-language", "zh-CN,zh;q=0.9"},
          {"Content-Type", "application/json"},
          {"User-Agent",
           R"(Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/119.0.6045.160 Safari/537.36)"},
      };
      cli.set_default_headers(headers);

      auto resp = cli.Post(endpoint.prefix, body.dump(), "application/json");
      if (respIsSucc(resp)) {
        co_return std::expected<::httplib::Result, std::string>{
            std::move(resp)};
      } else {
        XX_LOGE("http_post failed ({}): {}", url, resp->status);
        co_return std::unexpected{std::to_string(resp->status)};
      }
    } catch (const std::exception &e) {
      XX_LOGE("http_post failed ({}): {}", url, e.what());
      co_return std::unexpected{e.what()};
    }

    /*
    auto endpoint = neograph::async::split_async_endpoint(url);
    if (endpoint.prefix.empty()) {
      endpoint.prefix = "/";
    }
    auto headers = std::vector<std::pair<std::string, std::string>>{
        {"accept-language", "zh-CN,zh;q=0.9"},
        {"Content-Type", "application/json"},
    };
    neograph::async::RequestOptions opts;
    opts.timeout = timeout;
    opts.max_redirects = 5;

    auto ex = co_await asio::this_coro::executor;

    try {
      auto resp = co_await neograph::async::async_post(
          ex, endpoint.host, endpoint.port, endpoint.prefix, body.dump(),
          std::move(headers), endpoint.tls, opts);
      co_return std::expected<neograph::async::HttpResponse, std::string>{
          std::move(resp)};
    } catch (const std::exception &e) {
      XX_LOGE("http_post failed ({}): {}", url, e.what());
      co_return std::unexpected{e.what()};
    }
      */
  }

  static inline asio::awaitable<std::expected<std::string, std::string>>
  fetchMarkdown(const std::string &url,
                std::chrono::milliseconds timeout = std::chrono::seconds{15}) {
    // GET HTML
    auto resp = co_await getAsync(url);
    if (resp.has_value()) {
      if (respIsSucc(resp.value())) {
        // to markdown
        auto options = html2md::Options{
            .splitLines = false,
        };
        auto convert = html2md::Converter{resp.value().body, &options};
        co_return std::expected<std::string, std::string>{convert.convert()};
      } else {
        XX_LOGE("fetchMarkdown resp failed: StatuCode {}", resp.value().status);
      }
    }
    XX_LOGE("fetchMarkdown error: {}", resp.error());
    co_return std::unexpected{resp.error()};
  }
};
} // namespace util
} // namespace agentxx