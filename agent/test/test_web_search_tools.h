#pragma once

#include "neograph/neograph.h"
#include "tools/web_search.h"
#include <asio/awaitable.hpp>
#include <iostream>
#include <string>

namespace agentxx {
namespace test {

inline asio::awaitable<void> test_web_search_get_definition() {
  auto tool =
      agentxx::tools::WebSearchTool{"https://example.com/search?q={}", false};
  auto def = tool.get_definition();
  if (def.name == "web_search") {
    std::cout << "[PASS] WebSearchTool::get_definition() name correct"
              << std::endl;
  } else {
    std::cout << "[FAIL] WebSearchTool::get_definition() name incorrect"
              << std::endl;
  }
  co_return;
}

inline asio::awaitable<void> test_web_search_empty_query() {
  auto tool =
      agentxx::tools::WebSearchTool{"https://example.com/search?q={}", false};
  auto args = neograph::json{{"query", ""}};
  auto result = co_await tool.execute_async(args);
  if (result.find("\"error\"") != std::string::npos) {
    std::cout << "[PASS] WebSearchTool returns error for empty query"
              << std::endl;
  } else {
    std::cout
        << "[FAIL] WebSearchTool should return error for empty query, got: "
        << result << std::endl;
  }
  co_return;
}

inline asio::awaitable<void> test_web_search_definition_has_required_query() {
  auto tool =
      agentxx::tools::WebSearchTool{"https://example.com/search?q={}", false};
  auto def = tool.get_definition();
  auto &params = def.parameters;
  if (params.is_object() && params.contains("required") &&
      params["required"].is_array()) {
    auto required = params["required"];
    bool hasQuery = false;
    for (const auto &item : required) {
      if (item.is_string() && item.get<std::string>() == "query") {
        hasQuery = true;
        break;
      }
    }
    if (hasQuery) {
      std::cout
          << "[PASS] WebSearchTool definition has 'query' as required param"
          << std::endl;
    } else {
      std::cout << "[FAIL] WebSearchTool definition missing 'query' in required"
                << std::endl;
    }
  } else {
    std::cout << "[FAIL] WebSearchTool definition has no required params"
              << std::endl;
  }
  co_return;
}

inline asio::awaitable<void> test_web_fetch_url_get_definition() {
  auto tool = agentxx::tools::WebFetchUrlTool{};
  auto def = tool.get_definition();
  if (def.name == "web_fetch_url") {
    std::cout << "[PASS] WebFetchUrlTool::get_definition() name correct"
              << std::endl;
  } else {
    std::cout << "[FAIL] WebFetchUrlTool::get_definition() name incorrect"
              << std::endl;
  }
  co_return;
}

inline asio::awaitable<void> test_web_fetch_url_empty_url() {
  auto tool = agentxx::tools::WebFetchUrlTool{};
  auto args = neograph::json{{"url", ""}};
  auto result = co_await tool.execute_async(args);
  if (result.find("\"error\"") != std::string::npos) {
    std::cout << "[PASS] WebFetchUrlTool returns error for empty url"
              << std::endl;
  } else {
    std::cout
        << "[FAIL] WebFetchUrlTool should return error for empty url, got: "
        << result << std::endl;
  }
  co_return;
}

inline asio::awaitable<void> test_web_fetch_url_default_timeout() {
  auto tool = agentxx::tools::WebFetchUrlTool{};
  auto def = tool.get_definition();
  auto &params = def.parameters;
  if (params.is_object() && params.contains("properties") &&
      params["properties"].is_object()) {
    auto props = params["properties"];
    if (props.contains("timeout") && props["timeout"].is_object()) {
      std::cout << "[PASS] WebFetchUrlTool definition has timeout parameter"
                << std::endl;
    } else {
      std::cout << "[FAIL] WebFetchUrlTool definition missing timeout parameter"
                << std::endl;
    }
  }
  co_return;
}

inline asio::awaitable<void> test_web_fetch_url_markdown_get_definition() {
  auto tool = agentxx::tools::WebFetchUrlMarkdownTool{};
  auto def = tool.get_definition();
  if (def.name == "web_fetch_url_markdown") {
    std::cout << "[PASS] WebFetchUrlMarkdownTool::get_definition() name correct"
              << std::endl;
  } else {
    std::cout
        << "[FAIL] WebFetchUrlMarkdownTool::get_definition() name incorrect"
        << std::endl;
  }
  co_return;
}

inline asio::awaitable<void> test_web_fetch_url_markdown_empty_url() {
  auto tool = agentxx::tools::WebFetchUrlMarkdownTool{};
  auto args = neograph::json{{"url", ""}};
  auto result = co_await tool.execute_async(args);
  if (result.find("\"error\"") != std::string::npos) {
    std::cout << "[PASS] WebFetchUrlMarkdownTool returns error for empty url"
              << std::endl;
  } else {
    std::cout << "[FAIL] WebFetchUrlMarkdownTool should return error for empty "
                 "url, got: "
              << result << std::endl;
  }
  co_return;
}

inline asio::awaitable<void> test_web_fetch_url_markdown_description() {
  auto tool = agentxx::tools::WebFetchUrlMarkdownTool{};
  auto def = tool.get_definition();
  if (def.description.find("markdown") != std::string::npos ||
      def.description.find("Markdown") != std::string::npos) {
    std::cout << "[PASS] WebFetchUrlMarkdownTool description mentions markdown"
              << std::endl;
  } else {
    std::cout << "[FAIL] WebFetchUrlMarkdownTool description should mention "
                 "markdown"
              << std::endl;
  }
  co_return;
}

inline asio::awaitable<void> test_web_search_convert_html2markdown() {
  auto tool =
      agentxx::tools::WebSearchTool{"https://example.com/search?q={}", true};
  auto def = tool.get_definition();
  if (def.name == "web_search") {
    std::cout << "[PASS] WebSearchTool with convertHtml2markdown=true created "
                 "successfully"
              << std::endl;
  } else {
    std::cout << "[FAIL] WebSearchTool with convertHtml2markdown=true failed"
              << std::endl;
  }
  co_return;
}

inline asio::awaitable<void> run_web_search_tools_tests() {
  std::cout << "======= Test: Web Search Tools =======" << std::endl;

  auto run = [](auto testFn) -> asio::awaitable<void> {
    try {
      co_await testFn();
    } catch (const std::exception &e) {
      std::cout << "[FAIL] Exception in test: " << e.what() << std::endl;
    }
  };

  co_await run(test_web_search_get_definition);
  co_await run(test_web_search_empty_query);
  co_await run(test_web_search_definition_has_required_query);
  co_await run(test_web_search_convert_html2markdown);
  co_await run(test_web_fetch_url_get_definition);
  co_await run(test_web_fetch_url_empty_url);
  co_await run(test_web_fetch_url_default_timeout);
  co_await run(test_web_fetch_url_markdown_get_definition);
  co_await run(test_web_fetch_url_markdown_empty_url);
  co_await run(test_web_fetch_url_markdown_description);
  std::cout << "======= Test Done =======" << std::endl;
}

} // namespace test
} // namespace agentxx