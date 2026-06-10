#pragma once

#include "tools/sub_agent.h"
#include "tools/tool.h"
#include <filesystem>
#include <format>
#include <iostream>
#include <map>
#include <memory>
#include <neograph/llm/rate_limited_provider.h>
#include <neograph/llm/schema_provider.h>
#include <neograph/neograph.h>
#include <sstream>
#include <string>
#include <vector>

namespace agentxx {
namespace tools {

/// 当主模型在已加载的 tool和skill
/// 中未找到可以解决用户需求的方法时，可以尝试调用`tool_skill_search`查找可能可用的
/// tool 或 skill
/// - 需要实现区分 tool 是可延迟加载的，并区分已加载、未加载，未加载的 tool
/// 只需要在主模型的上下文中保留 toolname 即可
/// - 子模型搜索可用的 tool和skill 时，可以预先加载可能需要的 skill
/// 内容，然后对比后决定具体应当加载的 tool、skill 文件
class ToolSkillSearchTool : public ::agentxx::tools::SubAgentTaskBase {
protected:
  inline static constexpr auto defSystemPromptTemplate = std::string_view{R"(
You are an assistant that, based on user requirements, tries to find the appropriate tools and skills to load. 
You can load additional skill files, analyze the user's needs to determine usable tools and skills, 
and then output a JSON object in the format: `{"tool": ["tool_name_1", ""tool_name_2""], "skill": ["path/to/skill/file"]}`.
You can output multiple tools and skills at the same time. 
If no suitable tool is found, output an empty array; similarly, if no suitable skill is found, output an empty array. 
If neither tools nor skills are suitable, output `{"tool": [], "skill": []}`.

)"};

public:
  explicit ToolSkillSearchTool(
      const neograph::graph::NodeContext &in_context,
      const std::vector<::agentxx::tools::XXToolBase> &delayTools)
      : ::agentxx::tools::SubAgentTaskBase(
            "tool_skill_search",
            "Search available tool or skill for loading. "
            "(already set system prompt)",
            "") {
    createSubgraph(in_context);
    createSystemPrompt(delayTools);
  }

  asio::awaitable<void> onSubagentEnd(std::string &result) override {
    // 将 result 转json，读取 tool、skill，加载
    // 转json失败则不处理
  }

  void createSystemPrompt(
      const std::vector<::agentxx::tools::XXToolBase> &delayTools) {
    // TODO: 生成 system prompt
  }

  void createSubgraph(const neograph::graph::NodeContext &context) {
    if (nullptr == subgraph) {
      auto inner = neograph::graph::GraphEngine::compile(
          defCreateSubGraphDefine(), context);
      subgraph = std::shared_ptr<neograph::graph::GraphEngine>(inner.release());
    }
  }

  inline static neograph::json defCreateSubGraphDefine() {
    return neograph::json{
        {"name", "xx_ToolSkillSearch"},
        {
            "channels",
            {
                {
                    "messages",
                    {{"type", "list"}, {"reducer", "append"}},
                },
            },
        },
        {
            "nodes",
            {
                {
                    "agent_start",
                    {{
                        "type",
                        agentxx::nodes::AgentStartCallWrapNode::defNodeType,
                    }},
                },
                {
                    "agent_end",
                    {{
                        "type",
                        agentxx::nodes::MiddlewareWrapAgentEndCallNode::
                            defNodeType,
                    }},
                },
                {
                    "tools",
                    {{
                        "type",
                        agentxx::nodes::ToolcallWrapNode::defNodeType,
                    }},
                },
                {
                    "llm",
                    {{
                        "type",
                        agentxx::nodes::ModelCallWrapNode::defNodeType,
                    }},
                },
            },
        },
        {
            "edges",
            neograph::json::array({
                {{"from", "__start__"}, {"to", "llm"}},
                {{"from", "agent_start"}, {"to", "llm"}},
                {
                    {"from", "llm"},
                    {"type", "conditional"},
                    {"condition", "has_tool_calls"},
                    {"routes", {{"true", "tools"}, {"false", "agent_end"}}},
                },
                {{"from", "tools"}, {"to", "llm"}},
                {{"from", "agent_end"}, {"to", "__end__"}},
            }),
        },
    };
  }
};
} // namespace tools
} // namespace agentxx