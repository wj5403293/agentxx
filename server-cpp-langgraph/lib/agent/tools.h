#pragma once

#include "neograph/neograph.h"

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"

#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace agentxx {
namespace tools {

// =========================================================================
// URL helpers
// =========================================================================

static std::pair<std::string, std::string> split_url(const std::string &url) {
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
static std::string url_encode(const std::string &s) {
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
      if (c < 16) {
        out << '0';
      }
      out << (int)c;
      out << std::dec << std::hex; // sticky std::hex
    }
  }
  return out.str();
}

// =========================================================================
// Crawl4AIClient — thin wrapper around Crawl4AI's POST /md endpoint.
//
// /md accepts {url, f, q, c} and returns {markdown, success, ...}.
// We use the default `f=fit` extractor, no query-based BM25 filter.
// =========================================================================

class Crawl4AIClient {
public:
  explicit Crawl4AIClient(std::string base_url)
      : base_url_(std::move(base_url)) {}

  // Returns cleaned markdown of `url`. Throws on HTTP failure.
  std::string fetch_markdown(const std::string &url,
                             const std::string &query_hint = {}) const {
    neograph::json body = neograph::json::object();
    body["url"] = url;
    if (!query_hint.empty()) {
      body["f"] = "bm25";
      body["q"] = query_hint;
    }

    auto [host, prefix] = split_url(base_url_);
    httplib::Client cli(host);
    cli.set_read_timeout(60, 0);
    cli.set_connection_timeout(15, 0);

    auto res = cli.Post(prefix + "md", body.dump(), "application/json");

    if (!res) {
      throw std::runtime_error(
          "Crawl4AI connection failed: " + httplib::to_string(res.error()) +
          " (is the crawl4ai container running at " + base_url_ + "?)");
    }
    if (res->status != 200) {
      throw std::runtime_error("Crawl4AI /md returned HTTP " +
                               std::to_string(res->status) + ": " + res->body);
    }

    auto resp = neograph::json::parse(res->body);
    if (!resp.value("success", false)) {
      throw std::runtime_error("Crawl4AI reported failure for " + url);
    }
    return resp.value("markdown", std::string{});
  }

private:
  std::string base_url_;
};

// =========================================================================
// WebSearchTool — invokes Crawl4AI on duckduckgo.com/html to obtain a
// markdown rendering of DDG's result list, then trims for the LLM.
//
// Trade-off: this relies on DDG's HTML layout being reasonably stable.
// Good enough for a demo; a production port would plug Serper / Brave /
// Tavily behind the same Tool interface.
// =========================================================================

class WebSearchTool : public neograph::Tool {
public:
  explicit WebSearchTool(std::shared_ptr<Crawl4AIClient> client)
      : client_(std::move(client)) {}

  std::string get_name() const override { return "web_search"; }

  neograph::ChatTool get_definition() const override {
    return {
        "web_search",
        "Search the web for a query. Returns a markdown list of top "
        "results (URLs + titles + snippets). Call fetch_url on the "
        "most promising result for the full page body.",
        neograph::json{
            {"type", "object"},
            {
                "properties",
                {{
                    "query",
                    {{"type", "string"}, {"description", "Search query."}},
                }},
            },
            {"required", neograph::json::array({"query"})},
        },
    };
  }

  std::string execute(const neograph::json &arguments) override {
    std::string query = arguments.value("query", std::string{});
    if (query.empty()) {
      return R"({"error":"empty query"})";
    }
    std::string ddg_url = "https://duckduckgo.com/html/?q=" + url_encode(query);

    try {
      std::string md = client_->fetch_markdown(ddg_url, query);
      // DDG's HTML is noisy; cap aggressively. Researcher context
      // bloat propagates to supervisor via compressed summaries; keep
      // raw tool output small so there's less for the model to quote.
      if (md.size() > 3000)
        md.resize(3000);
      return md;
    } catch (const std::exception &e) {
      neograph::json err = {{
          "error",
          std::string("web_search failed: ") + e.what(),
      }};
      return err.dump();
    }
  }

private:
  std::shared_ptr<Crawl4AIClient> client_;
};

// =========================================================================
// FetchUrlTool — invokes Crawl4AI /md directly for a specific URL.
// =========================================================================

class FetchUrlTool : public neograph::Tool {
public:
  explicit FetchUrlTool(std::shared_ptr<Crawl4AIClient> client)
      : client_(std::move(client)) {}

  std::string get_name() const override { return "fetch_url"; }

  neograph::ChatTool get_definition() const override {
    return {
        "fetch_url",
        "Fetch a specific URL and return its cleaned markdown body. "
        "Use after web_search to read the full contents of a promising page.",
        neograph::json{
            {"type", "object"},
            {
                "properties",
                {{
                    "url",
                    {{"type", "string"},
                     {"description", "Absolute http/https URL."}},
                }},
            },
            {"required", neograph::json::array({"url"})},
        }};
  }

  std::string execute(const neograph::json &arguments) override {
    std::string url = arguments.value("url", std::string{});
    if (url.empty()) {
      return R"({"error":"empty url"})";
    }

    try {
      std::string md = client_->fetch_markdown(url);
      if (md.size() > 5000) {
        md.resize(5000);
      }
      return md;
    } catch (const std::exception &e) {
      neograph::json err = {{
          "error",
          std::string("fetch_url failed: ") + e.what(),
      }};
      return err.dump();
    }
  }

private:
  std::shared_ptr<Crawl4AIClient> client_;
};
}; // namespace tools
}; // namespace agentxx