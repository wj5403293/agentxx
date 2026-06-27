#pragma once

#include "agentxx/tools/string.h"
#include "neograph/neograph.h"
#include <asio/awaitable.hpp>
#include <iostream>
#include <string>

namespace agentxx {
namespace test {

inline asio::awaitable<void> test_html_to_markdown_basic() {
  auto tool = agentxx::tools::StringHtml2MarkdownTool{};
  auto def = tool.get_definition();
  if (def.name == "string_html_to_markdown") {
    std::cout << "[PASS] StringHtml2MarkdownTool::get_definition() name correct"
              << std::endl;
  } else {
    std::cout
        << "[FAIL] StringHtml2MarkdownTool::get_definition() name incorrect"
        << std::endl;
  }
  co_return;
}

inline asio::awaitable<void> test_html_to_markdown_empty_content() {
  auto tool = agentxx::tools::StringHtml2MarkdownTool{};
  auto args = neograph::json{{"content", ""}};
  auto result = co_await tool.execute_async(args);
  if (result.find("\"error\"") != std::string::npos) {
    std::cout
        << "[PASS] StringHtml2MarkdownTool returns error for empty content"
        << std::endl;
  } else {
    std::cout << "[FAIL] StringHtml2MarkdownTool should return error for empty "
                 "content, got: "
              << result << std::endl;
  }
  co_return;
}

inline asio::awaitable<void> test_html_to_markdown_convert() {
  auto tool = agentxx::tools::StringHtml2MarkdownTool{};
  auto args = neograph::json{{"content", "<h1>Hello</h1><p>World</p>"}};
  auto result = co_await tool.execute_async(args);
  if (result.find("# Hello") != std::string::npos &&
      result.find("World") != std::string::npos) {
    std::cout << "[PASS] StringHtml2MarkdownTool converts HTML to markdown"
              << std::endl;
  } else {
    std::cout << "[FAIL] StringHtml2MarkdownTool conversion failed, got: "
              << result << std::endl;
  }
  co_return;
}

inline asio::awaitable<void> test_html_to_markdown_simple_text() {
  auto tool = agentxx::tools::StringHtml2MarkdownTool{};
  auto args = neograph::json{{"content", "<b>bold</b> <i>italic</i>"}};
  auto result = co_await tool.execute_async(args);
  if (result.find("**bold**") != std::string::npos &&
      result.find("*italic*") != std::string::npos) {
    std::cout << "[PASS] StringHtml2MarkdownTool converts bold and italic"
              << std::endl;
  } else {
    std::cout << "[FAIL] StringHtml2MarkdownTool bold/italic conversion "
                 "failed, got: "
              << result << std::endl;
  }
  co_return;
}

inline asio::awaitable<void> test_regexp_get_definition() {
  auto tool = agentxx::tools::StringRegexpTool{};
  auto def = tool.get_definition();
  if (def.name == "string_regexp") {
    std::cout << "[PASS] StringRegexpTool::get_definition() name correct"
              << std::endl;
  } else {
    std::cout << "[FAIL] StringRegexpTool::get_definition() name incorrect"
              << std::endl;
  }
  co_return;
}

inline asio::awaitable<void> test_regexp_empty_content() {
  auto tool = agentxx::tools::StringRegexpTool{};
  auto args = neograph::json{
      {"content", ""},
      {"exps", neograph::json::array({"test"})},
      {"opt", "search"},
  };
  auto result = co_await tool.execute_async(args);
  if (result.find("\"error\"") != std::string::npos) {
    std::cout << "[PASS] StringRegexpTool returns error for empty content"
              << std::endl;
  } else {
    std::cout << "[FAIL] StringRegexpTool should return error for empty "
                 "content, got: "
              << result << std::endl;
  }
  co_return;
}

inline asio::awaitable<void> test_regexp_empty_exps() {
  auto tool = agentxx::tools::StringRegexpTool{};
  auto args = neograph::json{
      {"content", "some text"},
      {"exps", neograph::json::array()},
      {"opt", "search"},
  };
  auto result = co_await tool.execute_async(args);
  if (result.find("\"error\"") != std::string::npos) {
    std::cout << "[PASS] StringRegexpTool returns error for empty exps"
              << std::endl;
  } else {
    std::cout << "[FAIL] StringRegexpTool should return error for empty exps, "
                 "got: "
              << result << std::endl;
  }
  co_return;
}

inline asio::awaitable<void> test_regexp_empty_opt() {
  auto tool = agentxx::tools::StringRegexpTool{};
  auto args = neograph::json{
      {"content", "some text"},
      {"exps", neograph::json::array({"test"})},
      {"opt", ""},
  };
  auto result = co_await tool.execute_async(args);
  if (result.find("\"error\"") != std::string::npos) {
    std::cout << "[PASS] StringRegexpTool returns error for empty opt"
              << std::endl;
  } else {
    std::cout << "[FAIL] StringRegexpTool should return error for empty opt, "
                 "got: "
              << result << std::endl;
  }
  co_return;
}

inline asio::awaitable<void> test_regexp_invalid_opt() {
  auto tool = agentxx::tools::StringRegexpTool{};
  auto args = neograph::json{
      {"content", "some text"},
      {"exps", neograph::json::array({"test"})},
      {"opt", "invalid"},
  };
  auto result = co_await tool.execute_async(args);
  if (result.find("\"error\"") != std::string::npos &&
      result.find("invalid") != std::string::npos) {
    std::cout << "[PASS] StringRegexpTool returns error for invalid opt"
              << std::endl;
  } else {
    std::cout << "[FAIL] StringRegexpTool should return error for invalid opt, "
                 "got: "
              << result << std::endl;
  }
  co_return;
}

inline asio::awaitable<void> test_regexp_search_match() {
  auto tool = agentxx::tools::StringRegexpTool{};
  auto args = neograph::json{
      {"content", "hello world, hello everyone"},
      {"exps", neograph::json::array({"hello"})},
      {"opt", "search"},
  };
  auto result = co_await tool.execute_async(args);
  if (result.find("Match found") != std::string::npos &&
      result.find("hello") != std::string::npos) {
    std::cout << "[PASS] StringRegexpTool search finds matches" << std::endl;
  } else {
    std::cout << "[FAIL] StringRegexpTool search should find matches, got: "
              << result << std::endl;
  }
  co_return;
}

inline asio::awaitable<void> test_regexp_search_no_match() {
  auto tool = agentxx::tools::StringRegexpTool{};
  auto args = neograph::json{
      {"content", "hello world"},
      {"exps", neograph::json::array({"xyz_not_found"})},
      {"opt", "search"},
  };
  auto result = co_await tool.execute_async(args);
  if (result.find("No match found") != std::string::npos) {
    std::cout << "[PASS] StringRegexpTool search returns 'No match found' "
                 "when no match"
              << std::endl;
  } else {
    std::cout << "[FAIL] StringRegexpTool search should return 'No match "
                 "found', got: "
              << result << std::endl;
  }
  co_return;
}

inline asio::awaitable<void> test_regexp_replace() {
  auto tool = agentxx::tools::StringRegexpTool{};
  auto args = neograph::json{
      {"content", "hello world"},
      {"exps", neograph::json::array({"world"})},
      {"opt", "replace"},
      {"replace_str", "universe"},
  };
  auto result = co_await tool.execute_async(args);
  if (result.find("hello universe") != std::string::npos) {
    std::cout << "[PASS] StringRegexpTool replace works correctly" << std::endl;
  } else {
    std::cout << "[FAIL] StringRegexpTool replace failed, got: " << result
              << std::endl;
  }
  co_return;
}

inline asio::awaitable<void> test_regexp_replace_no_match() {
  auto tool = agentxx::tools::StringRegexpTool{};
  auto args = neograph::json{
      {"content", "hello world"},
      {"exps", neograph::json::array({"xyz_not_found"})},
      {"opt", "replace"},
      {"replace_str", "universe"},
  };
  auto result = co_await tool.execute_async(args);
  if (result.find("No match found") != std::string::npos) {
    std::cout << "[PASS] StringRegexpTool replace returns 'No match found' "
                 "when no match"
              << std::endl;
  } else {
    std::cout << "[FAIL] StringRegexpTool replace should return 'No match "
                 "found', got: "
              << result << std::endl;
  }
  co_return;
}

inline asio::awaitable<void> test_regexp_remove() {
  auto tool = agentxx::tools::StringRegexpTool{};
  auto args = neograph::json{
      {"content", "hello world, hello everyone"},
      {"exps", neograph::json::array({"hello "})},
      {"opt", "remove"},
  };
  auto result = co_await tool.execute_async(args);
  if (result.find("world") != std::string::npos &&
      result.find("hello") == std::string::npos) {
    std::cout << "[PASS] StringRegexpTool remove works correctly" << std::endl;
  } else {
    std::cout << "[FAIL] StringRegexpTool remove failed, got: " << result
              << std::endl;
  }
  co_return;
}

inline asio::awaitable<void> test_regexp_remove_no_match() {
  auto tool = agentxx::tools::StringRegexpTool{};
  auto args = neograph::json{
      {"content", "hello world"},
      {"exps", neograph::json::array({"xyz_not_found"})},
      {"opt", "remove"},
  };
  auto result = co_await tool.execute_async(args);
  if (result.find("No match found") != std::string::npos) {
    std::cout << "[PASS] StringRegexpTool remove returns 'No match found' "
                 "when no match"
              << std::endl;
  } else {
    std::cout << "[FAIL] StringRegexpTool remove should return 'No match "
                 "found', got: "
              << result << std::endl;
  }
  co_return;
}

inline asio::awaitable<void> test_regexp_search_multi_patterns() {
  auto tool = agentxx::tools::StringRegexpTool{};
  auto args = neograph::json{
      {"content", "apple banana cherry"},
      {"exps", neograph::json::array({"apple", "cherry"})},
      {"opt", "search"},
  };
  auto result = co_await tool.execute_async(args);
  if (result.find("apple") != std::string::npos &&
      result.find("cherry") != std::string::npos) {
    std::cout << "[PASS] StringRegexpTool search with multiple patterns"
              << std::endl;
  } else {
    std::cout << "[FAIL] StringRegexpTool multi-pattern search failed, got: "
              << result << std::endl;
  }
  co_return;
}

inline asio::awaitable<void> test_regexp_replace_multi_patterns() {
  auto tool = agentxx::tools::StringRegexpTool{};
  auto args = neograph::json{
      {"content", "apple banana apple"},
      {"exps", neograph::json::array({"apple"})},
      {"opt", "replace"},
      {"replace_str", "orange"},
  };
  auto result = co_await tool.execute_async(args);
  auto count = size_t{0};
  size_t pos = 0;
  while ((pos = result.find("orange", pos)) != std::string::npos) {
    count++;
    pos += 6;
  }
  if (count >= 2) {
    std::cout << "[PASS] StringRegexpTool replace replaces all occurrences"
              << std::endl;
  } else {
    std::cout << "[FAIL] StringRegexpTool replace should replace all, got: "
              << result << std::endl;
  }
  co_return;
}

inline asio::awaitable<void> run_string_tools_tests() {
  std::cout << "======= Test: String Tools =======" << std::endl;

  auto run = [](auto testFn) -> asio::awaitable<void> {
    try {
      co_await testFn();
    } catch (const std::exception &e) {
      std::cout << "[FAIL] Exception in test: " << e.what() << std::endl;
    }
  };

  co_await run(test_html_to_markdown_basic);
  co_await run(test_html_to_markdown_empty_content);
  co_await run(test_html_to_markdown_convert);
  co_await run(test_html_to_markdown_simple_text);
  co_await run(test_regexp_get_definition);
  co_await run(test_regexp_empty_content);
  co_await run(test_regexp_empty_exps);
  co_await run(test_regexp_empty_opt);
  co_await run(test_regexp_invalid_opt);
  co_await run(test_regexp_search_match);
  co_await run(test_regexp_search_no_match);
  co_await run(test_regexp_replace);
  co_await run(test_regexp_replace_no_match);
  co_await run(test_regexp_remove);
  co_await run(test_regexp_remove_no_match);
  co_await run(test_regexp_search_multi_patterns);
  co_await run(test_regexp_replace_multi_patterns);
  std::cout << "======= Test Done =======" << std::endl;
}

} // namespace test
} // namespace agentxx