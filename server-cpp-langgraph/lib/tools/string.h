#pragma once

#include "util/hyperscan.h"
#include "util/log.h"
#include "util/string_util.h"
#include <cstdlib>
#include <filesystem>
#include <format>
#include <html2md.h>
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

class StringHtml2MarkdownTool : public neograph::Tool {
public:
  explicit StringHtml2MarkdownTool() {}

  std::string get_name() const override { return "html_to_markdown"; }

  neograph::ChatTool get_definition() const override {
    return {
        "html_to_markdown",
        R"(HTML to markdown)",
        neograph::json{
            {"type", "object"},
            {
                "properties",
                {{
                    "content",
                    {
                        {"type", "string"},
                        {"description", "HTML content."},
                    },
                }},
            },
            {"required", neograph::json::array({"content"})},
        },
    };
  }

  std::string execute(const neograph::json &arguments) override {
    auto content = arguments.value("content", std::string{});
    if (content.empty()) {
      return R"({"error":"Arg `content` is empty"})";
    }

    auto options = html2md::Options{
        .splitLines = false,
    };
    auto convert = html2md::Converter{content, &options};
    return convert.convert();
  }
};

class StringRegexpTool : public neograph::Tool {
public:
  explicit StringRegexpTool() {}

  std::string get_name() const override { return "string_regexp"; }

  neograph::ChatTool get_definition() const override {
    return {
        "string_regexp",
        "text search or replace by regexp(Regular Expression)",
        neograph::json{
            {"type", "object"},
            {
                "properties",
                {
                    {
                        "content",
                        {
                            {"type", "string"},
                            {"description", "text content."},
                        },
                    },
                    {
                        "match_exps",
                        {
                            {"type", "array"},
                            {"items", {{"type", "string"}}},
                            {"description",
                             R"(`grep regexp` or `exact search string` array, depend on arg `match_type`.
Search succeeds if any of the provided array match.)"},
                        },
                    },
                    {
                        "match_regexp",
                        {
                            {"type", "boolean"},
                            {"description", "If true, arg `match_exps` should "
                                            "be regexp array, "
                                            "match the string using the "
                                            "regexps; otherwise, "
                                            "perform a literal exact match"},
                        },
                    },
                },
            },
            {
                "required",
                neograph::json::array(
                    {"content", "match_exps", "match_regexp"}),
            },
        },
    };
  }

  std::string execute(const neograph::json &arguments) override {
    auto content = arguments.value("content", std::string{});
    if (content.empty()) {
      return R"({"error":"Arg `content` is empty"})";
    }
    auto match_exps = arguments.value("match_exps", std::vector<std::string>{});
    if (match_exps.empty()) {
      return R"({"error":"Arg `match_exps` is empty"})";
    }
    auto match_regexp = arguments.value<bool>("match_regexp", false);

    if (match_regexp) {
    } else {
      auto list = std::vector<std::string_view>{};
      list.resize(match_exps.size());
      for (int i = 0; i < match_exps.size(); ++i) {
        list[i] = std::string_view{match_exps[i]};
      }
      auto searcher = agentxx::util::AhoCorasick{list, false};
      searcher.search(content);
    }
    return R"({"error":"No match found"})";
  }
};

} // namespace tools
} // namespace agentxx