#pragma once

#include "agentxx/expand/codegraph_manager.h"
#include "agentxx/tools/tool.h"
#include "fmt/format.h"
#include "neograph/neograph.h"
#include <memory>
#include <sstream>
#include <string>

namespace agentxx {
namespace tools {

class CodeGraphSearchTool : public XXToolBase {
protected:
  std::shared_ptr<agentxx::expand::CodeGraphManager> codegraph;

public:
  CodeGraphSearchTool(
      std::shared_ptr<agentxx::expand::CodeGraphManager> in_codegraph,
      std::weak_ptr<agentxx::agent::AgentContext> in_agentContext)
      : XXToolBase("codegraph_search", in_agentContext, true, true),
        codegraph(std::move(in_codegraph)) {}

  neograph::ChatTool get_definition() const override {
    return {
        "codegraph_search",
        "Search for code symbols (functions, classes, etc.) by name using "
        "codegraph index. Returns matched symbols with file locations and "
        "signatures.",
        neograph::json{
            {"type", "object"},
            {
                "properties",
                {
                    {
                        "query",
                        {
                            {"type", "string"},
                            {"description",
                             "Symbol name to search for (supports partial "
                             "match)."},
                        },
                    },
                    {
                        "limit",
                        {
                            {"type", "number"},
                            {"description",
                             "Max number of results to return, default 20."},
                        },
                    },
                },
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
    int limit = static_cast<int>(arguments.value("limit", 20.0));

    auto result = codegraph->searchSymbols(query, limit);
    if (!result.success) {
      co_return fmt::format(R"({{"error":"{}"}})", result.error);
    }

    auto json = neograph::json::array();
    for (const auto &node : result.nodes) {
      json.push_back({
          {"kind", codegraph::node_kind_str(node.kind)},
          {"name", node.name},
          {"qualified_name", node.qualified_name},
          {"file", node.file_path},
          {"line", node.line},
          {"signature", node.signature},
      });
    }
    co_return json.dump();
  }
};

class CodeGraphContextTool : public XXToolBase {
protected:
  std::shared_ptr<agentxx::expand::CodeGraphManager> codegraph;

public:
  CodeGraphContextTool(
      std::shared_ptr<agentxx::expand::CodeGraphManager> in_codegraph,
      std::weak_ptr<agentxx::agent::AgentContext> in_agentContext)
      : XXToolBase("codegraph_context", in_agentContext, true, true),
        codegraph(std::move(in_codegraph)) {}

  neograph::ChatTool get_definition() const override {
    return {
        "codegraph_context",
        "Get rich context for a code symbol: definition, callers, callees, "
        "and methods (for classes). Useful for understanding how a function "
        "or class is used in the codebase.",
        neograph::json{
            {"type", "object"},
            {
                "properties",
                {
                    {
                        "symbol",
                        {
                            {"type", "string"},
                            {"description",
                             "Symbol name to get context for (e.g. "
                             "'MyClass::myMethod')."},
                        },
                    },
                    {
                        "limit",
                        {
                            {"type", "number"},
                            {"description",
                             "Max results per category, default 10."},
                        },
                    },
                    {
                        "max_depth",
                        {
                            {"type", "number"},
                            {"description",
                             "Max call graph traversal depth, default 3."},
                        },
                    },
                },
            },
            {"required", neograph::json::array({"symbol"})},
        },
    };
  }

  asio::awaitable<std::string>
  execute_async(const neograph::json &arguments) override {
    std::string symbol = arguments.value("symbol", std::string{});
    if (symbol.empty()) {
      co_return R"({"error":"Arg `symbol` is empty"})";
    }
    int limit = static_cast<int>(arguments.value("limit", 10.0));
    int max_depth = static_cast<int>(arguments.value("max_depth", 3.0));

    auto result = codegraph->getSymbolContext(symbol, limit, max_depth);
    if (!result.success) {
      co_return fmt::format(R"({{"error":"{}"}})", result.error);
    }

    co_return result.context.dump();
  }
};

class CodeGraphCallersTool : public XXToolBase {
protected:
  std::shared_ptr<agentxx::expand::CodeGraphManager> codegraph;

public:
  CodeGraphCallersTool(
      std::shared_ptr<agentxx::expand::CodeGraphManager> in_codegraph,
      std::weak_ptr<agentxx::agent::AgentContext> in_agentContext)
      : XXToolBase("codegraph_callers", in_agentContext, true, true),
        codegraph(std::move(in_codegraph)) {}

  neograph::ChatTool get_definition() const override {
    return {
        "codegraph_callers",
        "Find all functions that call a given symbol. Traces the call graph "
        "backwards to find callers.",
        neograph::json{
            {"type", "object"},
            {
                "properties",
                {
                    {
                        "symbol",
                        {
                            {"type", "string"},
                            {"description", "Symbol name to find callers for."},
                        },
                    },
                    {
                        "max_depth",
                        {
                            {"type", "number"},
                            {"description", "Max traversal depth, default 3."},
                        },
                    },
                },
            },
            {"required", neograph::json::array({"symbol"})},
        },
    };
  }

  asio::awaitable<std::string>
  execute_async(const neograph::json &arguments) override {
    std::string symbol = arguments.value("symbol", std::string{});
    if (symbol.empty()) {
      co_return R"({"error":"Arg `symbol` is empty"})";
    }
    int max_depth = static_cast<int>(arguments.value("max_depth", 3.0));

    auto result = codegraph->getCallers(symbol, max_depth);
    if (!result.success) {
      co_return fmt::format(R"({{"error":"{}"}})", result.error);
    }
    co_return result.impact.dump();
  }
};

class CodeGraphCalleesTool : public XXToolBase {
protected:
  std::shared_ptr<agentxx::expand::CodeGraphManager> codegraph;

public:
  CodeGraphCalleesTool(
      std::shared_ptr<agentxx::expand::CodeGraphManager> in_codegraph,
      std::weak_ptr<agentxx::agent::AgentContext> in_agentContext)
      : XXToolBase("codegraph_callees", in_agentContext, true, true),
        codegraph(std::move(in_codegraph)) {}

  neograph::ChatTool get_definition() const override {
    return {
        "codegraph_callees",
        "Find all functions that a given symbol calls. Traces the call graph "
        "forward to find callees.",
        neograph::json{
            {"type", "object"},
            {
                "properties",
                {
                    {
                        "symbol",
                        {
                            {"type", "string"},
                            {"description", "Symbol name to find callees for."},
                        },
                    },
                    {
                        "max_depth",
                        {
                            {"type", "number"},
                            {"description", "Max traversal depth, default 3."},
                        },
                    },
                },
            },
            {"required", neograph::json::array({"symbol"})},
        },
    };
  }

  asio::awaitable<std::string>
  execute_async(const neograph::json &arguments) override {
    std::string symbol = arguments.value("symbol", std::string{});
    if (symbol.empty()) {
      co_return R"({"error":"Arg `symbol` is empty"})";
    }
    int max_depth = static_cast<int>(arguments.value("max_depth", 3.0));

    auto result = codegraph->getCallees(symbol, max_depth);
    if (!result.success) {
      co_return fmt::format(R"({{"error":"{}"}})", result.error);
    }
    co_return result.impact.dump();
  }
};

class CodeGraphImpactTool : public XXToolBase {
protected:
  std::shared_ptr<agentxx::expand::CodeGraphManager> codegraph;

public:
  CodeGraphImpactTool(
      std::shared_ptr<agentxx::expand::CodeGraphManager> in_codegraph,
      std::weak_ptr<agentxx::agent::AgentContext> in_agentContext)
      : XXToolBase("codegraph_impact", in_agentContext, true, true),
        codegraph(std::move(in_codegraph)) {}

  neograph::ChatTool get_definition() const override {
    return {
        "codegraph_impact",
        "Analyze the impact of modifying a symbol. Finds all downstream "
        "symbols that may be affected (callers, references).",
        neograph::json{
            {"type", "object"},
            {
                "properties",
                {
                    {
                        "symbol",
                        {
                            {"type", "string"},
                            {"description",
                             "Symbol name to analyze impact for."},
                        },
                    },
                    {
                        "max_depth",
                        {
                            {"type", "number"},
                            {"description", "Max traversal depth, default 5."},
                        },
                    },
                },
            },
            {"required", neograph::json::array({"symbol"})},
        },
    };
  }

  asio::awaitable<std::string>
  execute_async(const neograph::json &arguments) override {
    std::string symbol = arguments.value("symbol", std::string{});
    if (symbol.empty()) {
      co_return R"({"error":"Arg `symbol` is empty"})";
    }
    int max_depth = static_cast<int>(arguments.value("max_depth", 5.0));

    auto result = codegraph->getImpact(symbol, max_depth);
    if (!result.success) {
      co_return fmt::format(R"({{"error":"{}"}})", result.error);
    }
    co_return result.impact.dump();
  }
};

class CodeGraphStatusTool : public XXToolBase {
protected:
  std::shared_ptr<agentxx::expand::CodeGraphManager> codegraph;

public:
  CodeGraphStatusTool(
      std::shared_ptr<agentxx::expand::CodeGraphManager> in_codegraph,
      std::weak_ptr<agentxx::agent::AgentContext> in_agentContext)
      : XXToolBase("codegraph_status", in_agentContext, false, true),
        codegraph(std::move(in_codegraph)) {}

  neograph::ChatTool get_definition() const override {
    return {
        "codegraph_status",
        "Get codegraph index statistics: total nodes, edges, files, and "
        "circular dependency count.",
        neograph::json{
            {"type", "object"},
            {"properties", neograph::json::object()},
        },
    };
  }

  asio::awaitable<std::string>
  execute_async(const neograph::json &arguments) override {
    auto result = codegraph->getStatus();
    if (!result.success) {
      co_return fmt::format(R"({{"error":"{}"}})", result.error);
    }

    co_return fmt::format(
        R"({{"total_nodes":{},"total_edges":{},"total_files":{},"circular_deps":{}}})",
        result.total_nodes, result.total_edges, result.total_files,
        result.circular_deps);
  }
};

class CodeGraphIndexTool : public XXToolBase {
protected:
  std::shared_ptr<agentxx::expand::CodeGraphManager> codegraph;

public:
  CodeGraphIndexTool(
      std::shared_ptr<agentxx::expand::CodeGraphManager> in_codegraph,
      std::weak_ptr<agentxx::agent::AgentContext> in_agentContext)
      : XXToolBase("codegraph_index", in_agentContext, false, false),
        codegraph(std::move(in_codegraph)) {}

  neograph::ChatTool get_definition() const override {
    return {
        "codegraph_index",
        "Index a directory for code analysis. Analyzes source files and "
        "builds the symbol database for search and context queries.",
        neograph::json{
            {"type", "object"},
            {
                "properties",
                {
                    {
                        "path",
                        {
                            {"type", "string"},
                            {"description",
                             "Absolute path to the directory to index."},
                        },
                    },
                    {
                        "incremental",
                        {
                            {"type", "boolean"},
                            {"description",
                             "Default `true`. Only index changed files."},
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
    std::string path = arguments.value("path", std::string{});
    if (path.empty()) {
      co_return R"({"error":"Arg `path` is empty"})";
    }
    bool incremental = arguments.value("incremental", true);

    bool ok = codegraph->indexDirectory(path, incremental);
    auto status = codegraph->getStatus();

    if (ok) {
      co_return fmt::format(
          R"({{"success":true,"total_nodes":{},"total_edges":{},"total_files":{}}})",
          status.total_nodes, status.total_edges, status.total_files);
    } else {
      co_return R"({"error":"Indexing failed"})";
    }
  }
};

class CodeGraphPathTool : public XXToolBase {
protected:
  std::shared_ptr<agentxx::expand::CodeGraphManager> codegraph;

public:
  CodeGraphPathTool(
      std::shared_ptr<agentxx::expand::CodeGraphManager> in_codegraph,
      std::weak_ptr<agentxx::agent::AgentContext> in_agentContext)
      : XXToolBase("codegraph_path", in_agentContext, true, true),
        codegraph(std::move(in_codegraph)) {}

  neograph::ChatTool get_definition() const override {
    return {
        "codegraph_path",
        "Find the call chain path between two symbols in the call graph.",
        neograph::json{
            {"type", "object"},
            {
                "properties",
                {
                    {
                        "from",
                        {
                            {"type", "string"},
                            {"description", "Starting symbol name."},
                        },
                    },
                    {
                        "to",
                        {
                            {"type", "string"},
                            {"description", "Target symbol name."},
                        },
                    },
                    {
                        "max_depth",
                        {
                            {"type", "number"},
                            {"description", "Max search depth, default 10."},
                        },
                    },
                },
            },
            {"required", neograph::json::array({"from", "to"})},
        },
    };
  }

  asio::awaitable<std::string>
  execute_async(const neograph::json &arguments) override {
    std::string from = arguments.value("from", std::string{});
    std::string to = arguments.value("to", std::string{});
    if (from.empty() || to.empty()) {
      co_return R"({"error":"Args `from` and `to` are required"})";
    }
    int max_depth = static_cast<int>(arguments.value("max_depth", 10.0));

    auto result = codegraph->findPath(from, to, max_depth);
    if (!result.success) {
      co_return fmt::format(R"({{"error":"{}"}})", result.error);
    }

    auto json = neograph::json::array();
    for (const auto &node : result.path) {
      json.push_back({
          {"kind", codegraph::node_kind_str(node.kind)},
          {"name", node.name},
          {"file", node.file_path},
          {"line", node.line},
      });
    }
    co_return fmt::format(R"({{"path":{},"depth":{}}})", json.dump(),
                          result.path.size());
  }
};

} // namespace tools
} // namespace agentxx