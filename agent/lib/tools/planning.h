#pragma once

#include "middlewares/planning.h"
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

/// 两层任务规划，提高模型在长流程中的注意力
/// - 目标层：Mermaid stateDiagram-v2 状态图描述整体工作流（大方向、依赖关系）
/// - 执行层：近期 todo list 追踪当前及下一步任务（执行细节、经验总结）
// Two-level task planning to maintain focus in long workflows:
//   - Strategic: Mermaid stateDiagram-v2 captures the overall workflow
//     (high-level phases, dependencies, branching, recovery paths)
//   - Tactical: Near-term todo list tracks current + next-step tasks
//     (execution details, lessons learned, re-planning hints)
// Features:
//   - State diagram shows the big picture: what to do, in what order
//   - Todo items focus on immediate execution: what's happening now
//   - Persisted in agent state per thread
//   - Helps agent organize complex multi-step work
class WritePlanningTool : public neograph::AsyncTool {
protected:
  std::weak_ptr<agentxx::middleware::PlanningMiddlewareHandle> planningContext;

public:
  explicit WritePlanningTool(
      std::weak_ptr<agentxx::middleware::PlanningMiddlewareHandle>
          in_planningContext)
      : planningContext(in_planningContext) {}

  std::string get_name() const override { return "planning_write"; }

  neograph::ChatTool get_definition() const override {
    return {
        "planning_write",
        R"(Two-level task planning tool for complex multi-step work sessions.

=== Strategic Layer: `roadmap` (required) ===
A Mermaid stateDiagram-v2 that captures the OVERALL workflow — the big picture.
This is your roadmap: major phases, dependencies between tasks, error recovery
paths, and the start-to-finish flow. Update this diagram whenever the plan
changes (new tasks, completed phases, dead ends).

State diagram conventions:
- Use `[*]` for start/end pseudo-states
- Name state nodes like `phase_N_description` (e.g. `phase_1_search_codebase`)
- Status transitions: pending → in_progress → completed | failed
- Show branching: what happens on success vs failure
- Replace the entire diagram each call

=== Tactical Layer: `todos` (optional) ===
A short list of IMMEDIATE and NEXT-STEP tasks only. Do NOT list every state
from the diagram — only the tasks you are actively working on or about to
start. Each item records execution details, lessons learned, and issues
encountered to help with re-planning.

Example for a "fix a bug" workflow:
- roadmap:
```mermaid
stateDiagram-v2
    [*] --> phase_1_reproduce_bug
    phase_1_reproduce_bug --> phase_1_in_progress: start
    phase_1_in_progress --> phase_1_completed: reproduced
    phase_1_in_progress --> phase_1_failed: cannot reproduce
    phase_1_completed --> phase_2_locate_root_cause
    phase_2_locate_root_cause --> phase_2_in_progress: analyze
    phase_2_in_progress --> phase_2_completed: found cause
    phase_2_completed --> phase_3_implement_fix
    phase_3_implement_fix --> phase_3_in_progress: coding
    phase_3_in_progress --> phase_3_completed: fix works
    phase_3_completed --> [*]
```
- todos (only current + next):
[
  {"state":"in_progress", "content":"Reproduce the crash with provided stack trace",
   "summary":"Found that it crashes on null pointer at line 342"},
  {"state":"pending", "content":"Locate root cause by tracing the null pointer source"}
]
)",
        neograph::json{
            {"type", "object"},
            {"required", neograph::json::array({"roadmap"})},
            {
                "properties",
                {
                    {
                        "roadmap",
                        {
                            {"type", "string"},
                            {"description",
                             R"(STRATEGIC LAYER: Mermaid stateDiagram-v2 of the overall workflow.
This is the big-picture roadmap. Include ALL phases even if not yet started.
Each phase gets state nodes for its statuses (pending/in_progress/completed/failed)
with transitions showing dependencies and error recovery paths.
Use `[*]` for start/end. Replace the entire diagram each call.)"},
                        },
                    },
                    {
                        "todos",
                        {
                            {"type", "array"},
                            {"items", {{"type", "object"}}},
                            {"description",
                             R"(TACTICAL LAYER: Near-term task items.
Focus on what you are actively doing NOW and what comes NEXT.
Do NOT list all phases from the diagram — only immediate execution items.
Each item records what was tried, what worked, and what to watch out for.

Item struct:
{
    "state": "pending",   // enum: pending, in_progress, completed, failed
    "content": "",        // task description
    "summary": ""         // execution notes: methods tried, issues encountered,
                          // optimization suggestions for re-planning
}
)"},
                        },
                    },
                },
            },
        },
    };
  }

  asio::awaitable<std::string>
  execute_async(const neograph::json &arguments) override {
    auto thread_id = arguments.value("thread_id", std::string{});
    if (thread_id.empty()) {
      co_return R"({"error":"Toolcall inner exec failed, need `thread_id`"})";
    }

    auto roadmap = arguments.value("roadmap", std::string{});
    if (roadmap.empty()) {
      co_return R"({"error":"Arg `roadmap` is empty, must provide a stateDiagram-v2 planning string"})";
    }

    auto handlePtr = planningContext.lock();
    if (nullptr == handlePtr) {
      co_return R"({"error":"planningContext is null"})";
    }

    auto state = co_await handlePtr->getStateItem(thread_id);

    neograph::json planStore = neograph::json::object();
    planStore["roadmap"] = roadmap;
    if (arguments.contains("todos")) {
      planStore["todos"] = arguments["todos"];
    }
    state->plannings[thread_id] = planStore;

    co_return "success";
  }
};
} // namespace tools
} // namespace agentxx