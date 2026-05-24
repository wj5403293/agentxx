#pragma once

#include <filesystem>
#include <format>
#include <iostream>
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

class FilesystemGlobTool : public neograph::Tool {
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

  std::string execute(const neograph::json &arguments) override {}
};
} // namespace tools
} // namespace agentxx