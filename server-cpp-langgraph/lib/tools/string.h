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
        "text search,replace or remove by regexp(Regular Expression)",
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
                        "exps",
                        {
                            {"type", "array"},
                            {"items", {{"type", "string"}}},
                            {"description",
                             R"(grep regexp array. Search succeeds if any of the provided array match.)"},
                        },
                    },
                    {
                        "opt",
                        {
                            {"type", "string"},
                            {"enum", neograph::json::array(
                                         {"search", "replace", "remove"})},
                            {"description", R"(match operator.
`search` return match result(s).
`replace` replace match result(s) with `replace_str`. return result text content.
`remove` remove match result(s) from content. return result text content.
)"},
                        },
                    },
                    {
                        "replace_str",
                        {
                            {"type", "string"},
                            {"description", "Default empty string. When opt is "
                                            "`replace`, replace "
                                            "string for match result(s)."},
                        },
                    },
                },
            },
            {
                "required",
                neograph::json::array({"content", "exps", "opt"}),
            },
        },
    };
  }

  std::string execute(const neograph::json &arguments) override {
    auto content = arguments.value("content", std::string{});
    if (content.empty()) {
      return R"({"error":"Arg `content` is empty"})";
    }
    auto match_exps = arguments.value("exps", std::vector<std::string>{});
    if (match_exps.empty()) {
      return R"({"error":"Arg `exps` is empty"})";
    }
    auto match_opt = arguments.value("opt", std::string{});
    if (match_opt.empty()) {
      return R"({"error":"Arg `opt` is empty"})";
    }

    auto regex = agentxx::util::XXRegex{match_exps};
    if (match_opt == std::string_view{"search"}) {
      auto results = std::vector<agentxx::util::XXRegexMatchResult>{};
      if (regex.match(content, results)) {
        auto relist = neograph::json::array();
        for (size_t i = 0; i < results.size(); ++i) {
          const auto &item = results[i];
          relist.push_back(content.substr(item.start, item.end - item.start));
        }
        return neograph::json{
            {"tip", std::format("Match found {} items.", results.size())},
            {"result", relist},
        }
            .dump();
      }
    } else if (match_opt == std::string_view{"replace"}) {
      auto results = std::vector<agentxx::util::XXRegexMatchResult>{};
      auto restr = regex.replace(
          content, arguments.value("replace_str", std::string{}), results);
      if (false == results.empty()) {
        return restr;
      }
    } else if (match_opt == std::string_view{"remove"}) {
      auto results = std::vector<agentxx::util::XXRegexMatchResult>{};
      auto restr = regex.remove(content, results);
      if (false == results.empty()) {
        return restr;
      }
    } else {
      return R"({"error":"Arg `opt` is invalid"})";
    }
    return R"({"error":"No match found"})";
  }
};
} // namespace tools
} // namespace agentxx