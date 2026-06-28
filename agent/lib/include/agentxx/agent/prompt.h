#pragma once

#include "agentxx/util/util.h"
#include "fmt/format.h"
#include <map>
#include <string>
#include <vector>

namespace agentxx {
namespace agent {

class ToolPrompt {
public:
  std::string depict;
  std::map<std::string, std::string> args;
};

/// 提示词汇总
/// - 将大部分 system prompt,tool prompt 汇集，方便自定义配置、自循环更新等功能
class AgentPrompt {
public:
  std::string systemPrompt = "You are a helpful assistant.";

  std::string systemPlanningPrompt = R"_(
## Planning

You have access to the `planning_write` tool to help you manage and plan complex objectives.
Use this tool for complex objectives to ensure that you are tracking each necessary step.
This tool is very helpful for planning complex objectives, and for breaking down these larger complex objectives into smaller steps.

It is critical that you mark todos as completed as soon as you are done with a step. Do not batch up multiple steps before marking them as completed.
For simple objectives that only require a few steps, it is better to just complete the objective directly and NOT use this tool.
Writing roadmap and todos takes time and tokens, use it when it is helpful for managing complex many-step problems! But not for simple few-step requests.

### Important To-Do List Usage Notes to Remember

- The `planning_write` tool should never be called multiple times in parallel.
- Don't be afraid to revise the To-Do list as you go. New information may reveal new tasks that need to be done, or old tasks that are irrelevant.

### Finishing a task

When you finish all work, write your final answer in the message AFTER your last `planning_write` call — not in the same turn as that call. Start the final message with the substantive content the user asked for — the data, computation, summary, or analysis. The user wants the result, not confirmation that the work is done.
)_";

  inline static constexpr std::string_view systemSkillPromptSkillMetasInsertKey{
      "{Skill-Meta-Lists-Insert-Key}"};

  std::string systemSkillPrompt = R"_(
## Skills System

You have access to a skills library that provides specialized capabilities and domain knowledge.

**Available Skills:**

{Skill-Meta-Lists-Insert-Key}

**How to Use Skills (Progressive Disclosure):**

Skills follow a **progressive disclosure** pattern - you see their name and description above, but only read full instructions when needed:

1. **Recognize when a skill applies**: Check if the user's task matches a skill's description
2. **Read the skill's full instructions**: Use toolcall `filesystem_read_text_file` on the path shown in the skill list above.
   Pass `line_limit=1000` since the default of 100 lines is too small for most skill files.
3. **Follow the skill's instructions**: SKILL.md contains step-by-step workflows, best practices, and examples
4. **Access supporting files**: Skills may include helper scripts, configs, or reference docs - use absolute paths

**When to Use Skills:**
- User's request matches a skill's domain (e.g., "analyse X" -> `data-analyse` skill)
- You need specialized knowledge or structured workflows
- A skill provides proven patterns for complex tasks

**Executing Skill Scripts:**
Skills may contain Python scripts or other executable files. Always use absolute paths from the skill list.

**Example Workflow:**

User: "Can you analyse the latest developments in quantum computing?"

1. Check available skills -> See "data-analyse" skill with its path
2. Read the full skill file by toolcall: `filesystem_read_text_file`
3. Follow the skill's research workflow (search -> organize -> synthesize)
4. Use any helper scripts with absolute paths

Remember: Skills make you more capable and consistent. When in doubt, check if a skill exists for the task!
)_";

  /// toolcall
  std::map<std::string, const ToolPrompt> toolPrompt{
      {
          "execute_linux_command",
          ToolPrompt{
              .args =
                  {
                      {"command",
                       fmt::format(R"(Command to execute.
Current System is {}{}, please use linux shell/bash commands.)",
                                   agentxx::util::getSystemName(),
                                   agentxx::util::isRunningInWSL() ? "/(WSL)"
                                                                   : "")},
                      {"all_output", R"(Default `true`. 
`true`: Return all output.
`false`: Truncate Output. Only return the stdout and stderr output when the command faild.)"},
                  },
          },
      },
  };
};

} // namespace agent
} // namespace agentxx