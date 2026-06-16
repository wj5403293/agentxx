#pragma once

#include "agent/config.h"
#include "asio/co_spawn.hpp"
#include "asio/detached.hpp"
#include "asio/io_context.hpp"
#include "middlewares/planning.h"
#include "middlewares/skill.h"
#include "middlewares/summarization.h"
#include "neograph/llm/openai_provider.h"
#include "neograph/mcp/client.h"
#include "neograph/neograph.h"
#include "nodes/agentcall.h"
#include "nodes/modelcall.h"
#include "nodes/toolcall.h"
#include "tools/execute_command.h"
#include "tools/filesystem.h"
#include "tools/get_current_datetime.h"
#include "tools/planning.h"
#include "tools/share_store.h"
#include "tools/skill.h"
#include "tools/string.h"
#include "tools/sub_agent.h"
#include "tools/tool_skill_search.h"
#include "tools/web_search.h"
#include "util/log.h"
#include <format>
#include <functional>
#include <iostream>
#include <memory>

namespace agentxx {

/// 单个训练测试用例
struct TrainingTestCase {
  std::string name;           // 用例名称，用于日志
  std::string input;          // 用户输入文本
  std::string expectedOutput; // 可选：期望输出，供评分参考
  neograph::json extra;       // 可选：额外上下文数据
};

/// 评分结果
struct TrainingScore {
  double score = 0.0;   // 评分 0.0 ~ 1.0
  std::string feedback; // 评分反馈文本
  bool passed = false;  // 是否通过（score >= 阈值）
  int iteration = 0;    // 当前迭代次数
  neograph::json extra; // 可选：额外评分数据
};

/// 训练调整回调：根据评分结果调整 agent 行为
/// - score: 当前评分
/// - testCase: 当前测试用例
/// - systemPrompt: 可修改的 system prompt
/// - extraMessages: 可追加的上下文消息（会注入到下一轮 agent 调用的 messages
/// 前）
using TrainingAdjustmentFunc = std::function<asio::awaitable<void>(
    const TrainingScore &score, const TrainingTestCase &testCase,
    std::string &systemPrompt,
    std::vector<neograph::ChatMessage> &extraMessages)>;

/// 自定义评分回调：替代默认 subagent 评分器
/// - agentOutput: agent 本轮输出文本
/// - testCase: 当前测试用例
/// - iteration: 当前迭代次数
using TrainingScoringFunc = std::function<asio::awaitable<TrainingScore>(
    const std::string &agentOutput, const TrainingTestCase &testCase,
    int iteration)>;

/// 迭代观察回调：每轮迭代结束后调用
/// - score: 当前评分
/// - agentOutput: agent 输出
using TrainingIterationCallback = std::function<void(
    const TrainingScore &score, const std::string &agentOutput)>;

/// 训练模式配置
struct TrainingConfig {
  /// 测试用例列表
  std::vector<TrainingTestCase> testCases;

  /// 评分 subagent 的 system prompt（定义评分标准）
  /// 默认为通用评分模板，会被填入 agent 输出和用例信息
  std::string scoringPrompt = R"(
You are an expert evaluator. Your task is to score the agent's response based on the test case.

Scoring criteria (0.0 to 1.0):
- 1.0: Perfect response, fully satisfies the test case requirements
- 0.8-0.9: Good response, minor issues
- 0.6-0.7: Acceptable response, some issues
- 0.4-0.5: Partially correct, significant issues
- 0.0-0.3: Poor or incorrect response

Output ONLY a JSON object with the following schema:
{
  "score": <0.0-1.0>,
  "feedback": "<detailed feedback explaining the score>",
  "passed": <true if score >= threshold, false otherwise>
}
)";

  /// 评分模型名称（为空则使用主 agent 的模型）
  std::string scoringModelName;

  /// 评分模型 API Key（为空则使用主 agent 的）
  std::string scoringModelApiKey;

  /// 评分模型 Base URL（为空则使用主 agent 的）
  std::string scoringModelBaseUrl;

  /// 最大迭代次数（每个测试用例）
  int maxIterations = 5;

  /// 收敛阈值：score >= 此值视为通过
  double convergenceThreshold = 0.8;

  /// 自定义调整处理回调（为空则不做调整，仅迭代）
  TrainingAdjustmentFunc adjustmentFunc;

  /// 自定义评分回调（为空则使用默认 subagent 评分器）
  TrainingScoringFunc scoringFunc;

  /// 每轮迭代观察回调
  TrainingIterationCallback onIteration;

  /// 是否启用训练模式日志输出
  bool verbose = true;
};

class DeepAgent {
protected:
  /// 主协程调度器
  /// - 不要在同线程中传递到 [runCliAsync]
  /// 内使用，因为[engine->run_stream_async]会启动其他 io_context， 交替 ioCtx
  /// 的话会导致互相等待，进而卡住
  /// - 某个异步函数需要 io_context 时，可以通过 `co_await
  /// asio::this_coro::executor` 获取当前异步函数运行时绑定的 io_context
  std::shared_ptr<asio::io_context> ioCtx = nullptr;
  std::unique_ptr<neograph::graph::GraphEngine> engine = nullptr;
  std::shared_ptr<agentxx::AgentxxConfig_c> config = nullptr;
  std::shared_ptr<agentxx::middleware::MiddlewareWarpHandleContext>
      middlewareHandleContext = nullptr;
  agentxx::tools::SubAgentManagerTool *subagentManagerToolPtr = nullptr;

public:
  DeepAgent(std::shared_ptr<agentxx::AgentxxConfig_c> in_config)
      : config(in_config) {
    assert(nullptr != config);
    assert(config->modelOpenAIBaseUrl.empty() == false);
    ioCtx = std::make_shared<asio::io_context>();
  }

  void init() {
    neograph::llm::OpenAIProvider::Config provideConfig{
        .api_key = config->modelOpenAIApiKey,
        .base_url = config->modelOpenAIBaseUrl,
        .default_model = config->modelOpenAIModelName,
    };

    {
      /// register Node
      neograph::graph::NodeFactory::instance().register_type(
          std::string{agentxx::nodes::AgentStartCallWrapNode::defNodeType},
          [this](const std::string &name, const neograph::json &,
                 const neograph::graph::NodeContext &ctx) {
            return std::make_unique<agentxx::nodes::AgentStartCallWrapNode>(
                name, middlewareHandleContext);
          });
      neograph::graph::NodeFactory::instance().register_type(
          std::string{
              agentxx::nodes::MiddlewareWrapAgentEndCallNode::defNodeType},
          [this](const std::string &name, const neograph::json &,
                 const neograph::graph::NodeContext &ctx) {
            return std::make_unique<
                agentxx::nodes::MiddlewareWrapAgentEndCallNode>(
                name, middlewareHandleContext);
          });
      neograph::graph::NodeFactory::instance().register_type(
          std::string{agentxx::nodes::ModelCallWrapNode::defNodeType},
          [this](const std::string &name, const neograph::json &,
                 const neograph::graph::NodeContext &ctx) {
            return std::make_unique<agentxx::nodes::ModelCallWrapNode>(
                name, ctx, middlewareHandleContext);
          });
      neograph::graph::NodeFactory::instance().register_type(
          std::string{agentxx::nodes::ToolcallWrapNode::defNodeType},
          [this](const std::string &name, const neograph::json &,
                 const neograph::graph::NodeContext &ctx) {
            return std::make_unique<agentxx::nodes::ToolcallWrapNode>(
                name, ctx, middlewareHandleContext);
          });
    }

    /// middleware
    middlewareHandleContext =
        std::make_shared<agentxx::middleware::MiddlewareWarpHandleContext>();
    auto subagentManagerTool =
        std::make_unique<agentxx::tools::SubAgentManagerTool>(
            "subagent_manager");
    subagentManagerToolPtr = subagentManagerTool.get();
    {
      {
        auto skillMiddleware =
            std::make_shared<agentxx::middleware::SkillMiddlewareHandle>(
                config->skillDirPaths, middlewareHandleContext);
        // skillMiddleware->toolcalls.push_back(
        //     std::make_unique<agentxx::tools::SkillTool>());
        middlewareHandleContext->handles.push_back(skillMiddleware);
      }

      {
        auto summarizationMiddleware = std::make_shared<
            agentxx::middleware::SummarizationMiddlewareHandle>(
            subagentManagerTool.get(), middlewareHandleContext, 256 * 1024);
        middlewareHandleContext->handles.push_back(summarizationMiddleware);
      }

      {
        auto todolistMiddleware =
            std::make_shared<agentxx::middleware::PlanningMiddlewareHandle>(
                middlewareHandleContext);
        todolistMiddleware->toolcalls.push_back(
            std::make_unique<agentxx::tools::WritePlanningTool>(
                todolistMiddleware));
        middlewareHandleContext->handles.push_back(todolistMiddleware);
      }

      /// Toolcall  应当作为最后一层
      middlewareHandleContext->handles.push_back(
          std::make_shared<agentxx::middleware::MiddlewareWarpHandle<
              agentxx::middleware::BaseMiddlewareState_c>>(
              "toolcall_log", middlewareHandleContext,
              (agentxx::middleware::onGraphNodeBeforeCallFunc) nullptr,
              (agentxx::middleware::onGraphNodeAfterCallFunc) nullptr,
              (agentxx::middleware::onGraphNodeBeforeCallFunc) nullptr,
              (agentxx::middleware::onGraphNodeAfterCallFunc) nullptr,
              [](neograph::graph::NodeInput &in) -> asio::awaitable<void> {
                return agentxx::nodes::ToolcallWrapNode::
                    defStdoutLogOnToolcallStart(in);
              },
              [](const neograph::graph::NodeInput &in,
                 neograph::graph::NodeOutput &result) {
                return agentxx::nodes::ToolcallWrapNode::
                    defStdoutLogOnToolcallEnd(in, result);
              }));
    }

    /// Toolcall
    std::vector<std::unique_ptr<agentxx::tools::XXToolBase>> tools{};
    {
      /// middleware tools
      for (auto &item : middlewareHandleContext->handles) {
        if (false == item->toolcalls.empty()) {
          tools.insert(tools.end(),
                       std::make_move_iterator(item->toolcalls.begin()),
                       std::make_move_iterator(item->toolcalls.end()));
        }
      }
    }
    {
      /// MCP tool
      for (auto &url : config->mcpServerUrls) {
        auto mcpClient = neograph::mcp::MCPClient{url};
        if (mcpClient.initialize(config->agentName)) {
          auto mcpTools = mcpClient.get_tools();
          XX_LOGD("append mcp tool size: {}", mcpTools.size());
          for (auto &tool : mcpTools) {
            tools.push_back(std::make_unique<agentxx::tools::XXToolWarp>(
                true, std::move(tool)));
          }
        }
      }
    }
    {
      tools.push_back(std::make_unique<agentxx::tools::ShareKVStoreTool>(
          middlewareHandleContext));
      tools.push_back(
          std::make_unique<agentxx::tools::FileSystemListFileTool>());
      tools.push_back(
          std::make_unique<agentxx::tools::FilesystemReadTextFileTool>());
      tools.push_back(
          std::make_unique<agentxx::tools::FilesystemReadBinaryFileTool>());
      tools.push_back(
          std::make_unique<agentxx::tools::FilesystemWriteFileTool>());
      tools.push_back(
          std::make_unique<agentxx::tools::FilesystemEditTextFileTool>());
      tools.push_back(std::make_unique<agentxx::tools::FilesystemGlobTool>());
      tools.push_back(std::make_unique<agentxx::tools::FilesystemGrepTool>());

      tools.push_back(
          std::make_unique<agentxx::tools::StringHtml2MarkdownTool>());
      tools.push_back(std::make_unique<agentxx::tools::StringRegexpTool>());

      if (false == config->websearchApiUrl.empty()) {
        tools.push_back(std::make_unique<agentxx::tools::WebSearchTool>(
            config->websearchApiUrl, config->websearchConvertHtml2markdown));
      }
      tools.push_back(std::make_unique<agentxx::tools::WebFetchUrlTool>());
      tools.push_back(
          std::make_unique<agentxx::tools::WebFetchUrlMarkdownTool>());

      tools.push_back(
          std::make_unique<agentxx::tools::GetCurrentDateTimeTool>());

#if IS_WIN_D
      tools.push_back(
          std::make_unique<agentxx::tools::ExecuteWindowsCommandTool>());
#elif IS_LINUX_D
      tools.push_back(
          std::make_unique<agentxx::tools::ExecuteLinuxCommandTool>());
      if (agentxx::util::isRunningInWSL()) {
        tools.push_back(
            std::make_unique<agentxx::tools::ExecuteWindowsCommandTool>());
      }
#endif

      {
        // subagent
        // {
        //   // tool_skill_search
        //   // - 复制 除了 subagent 的所有 tool/mcp tool/skill 组合上下文到
        //   // subagent 中 根据需求分析加载/使用的 tool/skill
        //   neograph::graph::NodeContext nodeContext{};
        //   nodeContext.instructions = "";
        //   nodeContext.provider =
        //       neograph::llm::OpenAIProvider::create_shared(provideConfig);

        //   // 收集延迟加载的 tool 信息
        //   std::vector<agentxx::tools::ToolSkillSearchSubAgentTask::DelayToolInfo>
        //       delayToolInfos;
        //   for (auto &t : tools) {
        //     auto *xxTool = dynamic_cast<agentxx::tools::XXToolBase
        //     *>(t.get()); if (xxTool && xxTool->isDelayLoad) {
        //       auto def = xxTool->get_definition();
        //       delayToolInfos.push_back({xxTool->get_name(),
        //       def.description});
        //     }
        //   }

        //   // 给子 agent 提供文件系统 tool，用于搜索和读取 SKILL.md
        //   std::vector<neograph::Tool *> searchToolPtrs;
        //   searchToolPtrs.reserve(tools.size());
        //   for (auto &t : tools) {
        //     const auto &name = t->get_name();
        //     if (name == "filesystem_glob" || name == "filesystem_listfile" ||
        //         name == "filesystem_read_text_file" ||
        //         name == "filesystem_grep") {
        //       searchToolPtrs.push_back(t.get());
        //     }
        //   }
        //   nodeContext.tools = std::move(searchToolPtrs);

        //   subagentManagerTool->subAgentList.insert(std::make_pair(
        //       "tool_skill_search",
        //       std::make_shared<agentxx::tools::ToolSkillSearchSubAgentTask>(
        //           nodeContext, delayToolInfos, config->skillDirPaths,
        //           middlewareHandleContext)));
        // }
        {
          // subagent_task
          neograph::graph::NodeContext nodeContext{};
          nodeContext.instructions = "";
          nodeContext.provider =
              neograph::llm::OpenAIProvider::create_shared(provideConfig);

          /// 复制 tool
          std::vector<neograph::Tool *> toolPtrs;
          toolPtrs.reserve(tools.size());
          for (auto &t : tools) {
            toolPtrs.push_back(t.get());
          }
          nodeContext.tools = std::move(toolPtrs);

          const auto nodeName = std::string{"subagent_task"};

          subagentManagerTool->subAgentList.insert(std::make_pair(
              nodeName, std::make_shared<agentxx::tools::SubAgentNormalTask>(
                            nodeName,
                            "Create a isolation messages context sub agent to "
                            "exec. (need system prompt)",
                            nodeContext)));
        }

        // {
        //   // training_scorer: 训练模式专用评分 subagent，无需工具
        //   neograph::graph::NodeContext scorerCtx{};
        //   scorerCtx.instructions = "";
        //   scorerCtx.provider =
        //       neograph::llm::OpenAIProvider::create_shared(provideConfig);

        //   const auto scorerName = std::string{"training_scorer"};

        //   subagentManagerTool->subAgentList.insert(std::make_pair(
        //       scorerName,
        //       std::make_shared<agentxx::tools::SubAgentNormalTask>(
        //           scorerName,
        //           "Scoring sub-agent for training mode: evaluates agent "
        //           "responses against test case criteria",
        //           scorerCtx)));
        // }

        tools.push_back(std::move(subagentManagerTool));
      }
    }

    /// Main Agent
    neograph::graph::NodeContext nodeContext{};
    nodeContext.instructions = config->systemPrompt;
    nodeContext.provider =
        neograph::llm::OpenAIProvider::create_shared(provideConfig);

    std::vector<neograph::Tool *> toolPtrs;
    toolPtrs.reserve(tools.size());
    for (auto &t : tools) {
      toolPtrs.push_back(t.get());
    }
    nodeContext.tools = std::move(toolPtrs);

    auto store = std::make_shared<neograph::graph::InMemoryCheckpointStore>();

    // JSON definition equivalent to the Agent::run() ReAct loop:
    //                 ------- sub_agent_task <--- toolcall/sub_agent_task
    //                 |                            |
    //                 |<---------------------------|
    //                 |                            |
    //                 v                            |
    //  __start__  -> llm ->  has_tool_calls  ->  tools
    //                               |
    //                               v
    //                            __end__
    auto graphDefinition = neograph::json{
        {"name", config->agentName},
        {
            "channels",
            {
                {"messages", {{"type", "list"}, {"reducer", "append"}}},
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
                {{"from", "__start__"}, {"to", "agent_start"}},
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

    engine = neograph::graph::GraphEngine::compile(graphDefinition, nodeContext,
                                                   store);
    {
      auto crudeTools = std::vector<std::unique_ptr<neograph::Tool>>{};
      for (auto &tool : tools) {
        crudeTools.push_back(std::move(tool));
      }
      engine->own_tools(std::move(crudeTools));
    }
  }

  asio::awaitable<void> runCliAsync() {
    std::cout << ">>> " << std::flush;

    for (std::string line; std::getline(std::cin, line);) {
      if (false == line.empty()) {
        try {
          neograph::graph::RunConfig cfg{
              .thread_id = "session",
              .input = {{
                  "messages",
                  neograph::json::array({{
                      {"role", "user"},
                      {"content", line},
                  }}),
              }},
              .resume_if_exists = false,
          };

          std::cout << config->agentNameView << ": " << std::flush;
          auto result = co_await engine->run_stream_async(
              cfg, [](const neograph::graph::GraphEvent &event) {
                switch (event.type) {
                case neograph::graph::GraphEvent::Type::NODE_START:
                case neograph::graph::GraphEvent::Type::NODE_END:
                  break;
                case neograph::graph::GraphEvent::Type::LLM_TOKEN: {
                  std::cout << event.data.get<std::string>() << std::flush;
                } break;
                case neograph::graph::GraphEvent::Type::CHANNEL_WRITE:
                case neograph::graph::GraphEvent::Type::INTERRUPT:
                case neograph::graph::GraphEvent::Type::ERROR:
                  break;
                }
              });
        } catch (const std::exception &e) {
          XX_LOGE(R"({{"error": "Agent Response failed: {}"}})", e.what());
        }
      }
      std::cout << "\n\n>>> ";
    }
  }

  void runCli() {
    asio::co_spawn(
        *ioCtx,
        [this]() -> asio::awaitable<void> { co_return co_await runCliAsync(); },
        asio::detached);
    ioCtx->run();
  }

  // ======================== 训练模式 ========================

  /// 以单条用户输入运行 agent，返回完整输出文本（非流式）
  /// - threadId: 会话线程 ID
  /// - userInput: 用户输入文本
  /// - systemPrompt: 可选的 system prompt（为空则使用默认）
  /// - extraMessages: 可选的额外上下文消息
  //   asio::awaitable<std::string> runSingleInputAsync(
  //       const std::string &threadId, const std::string &userInput,
  //       const std::string &systemPrompt = "",
  //       const std::vector<neograph::ChatMessage> &extraMessages = {}) {
  //     auto messages = neograph::json::array();

  //     if (false == systemPrompt.empty()) {
  //       messages.push_back({
  //           {"role", "system"},
  //           {"content", systemPrompt},
  //       });
  //     }

  //     for (const auto &msg : extraMessages) {
  //       neograph::json msgJson;
  //       neograph::to_json(msgJson, msg);
  //       messages.push_back(std::move(msgJson));
  //     }

  //     messages.push_back({
  //         {"role", "user"},
  //         {"content", userInput},
  //     });

  //     neograph::graph::RunConfig cfg{
  //         .thread_id = threadId,
  //         .input = {{"messages", std::move(messages)}},
  //         .resume_if_exists = false,
  //     };

  //     std::ostringstream oss;
  //     co_await engine->run_stream_async(
  //         cfg, [&oss](const neograph::graph::GraphEvent &event) {
  //           switch (event.type) {
  //           case neograph::graph::GraphEvent::Type::LLM_TOKEN: {
  //             oss << event.data.get<std::string>();
  //           } break;
  //           default:
  //             break;
  //           }
  //         });
  //     co_return oss.str();
  //   }

  /// 默认评分器：使用 subagent 进行评分
  //   asio::awaitable<TrainingScore>
  //   defaultScoringWithSubAgent(const std::string &agentOutput,
  //                              const TrainingTestCase &testCase, int
  //                              iteration, const TrainingConfig &trainCfg) {
  //     TrainingScore result;
  //     result.iteration = iteration;

  //     neograph::llm::OpenAIProvider::Config scorerProviderCfg{
  //         .api_key = trainCfg.scoringModelApiKey.empty()
  //                        ? config->modelOpenAIApiKey
  //                        : trainCfg.scoringModelApiKey,
  //         .base_url = trainCfg.scoringModelBaseUrl.empty()
  //                         ? config->modelOpenAIBaseUrl
  //                         : trainCfg.scoringModelBaseUrl,
  //         .default_model = trainCfg.scoringModelName.empty()
  //                              ? config->modelOpenAIModelName
  //                              : trainCfg.scoringModelName,
  //     };

  //     auto scorerProvider =
  //         neograph::llm::OpenAIProvider::create_shared(scorerProviderCfg);

  //     std::ostringstream scoringMessage;
  //     scoringMessage << "Test Case: " << testCase.name << "\n";
  //     scoringMessage << "User Input: " << testCase.input << "\n";
  //     if (false == testCase.expectedOutput.empty()) {
  //       scoringMessage << "Expected Output: " << testCase.expectedOutput <<
  //       "\n";
  //     }
  //     scoringMessage << "\nAgent Response:\n" << agentOutput << "\n";
  //     scoringMessage << "\nScore threshold (score >= "
  //                    << trainCfg.convergenceThreshold << " is passed)";

  //     neograph::ChatMessage scoringMsg{
  //         .role = "user",
  //         .content = scoringMessage.str(),
  //     };

  //     neograph::ChatMessage systemMsg{
  //         .role = "system",
  //         .content = trainCfg.scoringPrompt,
  //     };

  //     try {
  //       std::vector<neograph::ChatMessage> messages = {systemMsg,
  //       scoringMsg}; auto response = co_await scorerProvider->chat_async(
  //           messages, neograph::json::object());

  //       const auto &content = response.content;

  //       auto parsed = neograph::json::parse(content);
  //       if (parsed.is_object()) {
  //         result.score = parsed.value("score", 0.0);
  //         result.feedback = parsed.value("feedback", std::string{});
  //         result.passed = parsed.value("passed", false);
  //         if (parsed.contains("extra") && parsed["extra"].is_object()) {
  //           result.extra = parsed["extra"];
  //         }
  //       } else {
  //         result.score = 0.0;
  //         result.feedback = "Scorer returned non-JSON: " + content;
  //         result.passed = false;
  //       }
  //     } catch (const std::exception &e) {
  //       result.score = 0.0;
  //       result.feedback = std::string("Scoring subagent error: ") + e.what();
  //       result.passed = false;
  //     }

  //     co_return result;
  //   }

  /// 训练模式主循环：对所有测试用例执行 输入→评分→调整→再评分 迭代
  /// 返回每个测试用例的最终评分历史
  //   asio::awaitable<std::vector<std::vector<TrainingScore>>>
  //   runTrainingMode(const TrainingConfig &trainCfg) {
  //     std::vector<std::vector<TrainingScore>> allResults;
  //     allResults.reserve(trainCfg.testCases.size());

  //     for (size_t caseIdx = 0; caseIdx < trainCfg.testCases.size();
  //     ++caseIdx) {
  //       const auto &testCase = trainCfg.testCases[caseIdx];

  //       if (trainCfg.verbose) {
  //         XX_LOGD("[Training] ====== Test Case [{}/{}]: {} ======", caseIdx +
  //         1,
  //                 trainCfg.testCases.size(), testCase.name);
  //       }

  //       std::vector<TrainingScore> caseScores;
  //       std::string currentSystemPrompt = config->systemPrompt;
  //       std::vector<neograph::ChatMessage> extraMessages;
  //       std::string lastOutput;

  //       for (int iter = 0; iter < trainCfg.maxIterations; ++iter) {
  //         const auto threadId =
  //             std::format("training_{}_{}", caseIdx, testCase.name);

  //         if (trainCfg.verbose) {
  //           XX_LOGD("[Training] Case '{}' Iteration {}/{}", testCase.name,
  //                   iter + 1, trainCfg.maxIterations);
  //         }

  //         // Step 1: Agent 运行
  //         lastOutput = co_await runSingleInputAsync(
  //             threadId, testCase.input, currentSystemPrompt, extraMessages);

  //         if (trainCfg.verbose) {
  //           XX_LOGD("[Training] Case '{}' Iter {} Output (len={}): {}",
  //                   testCase.name, iter + 1, lastOutput.size(),
  //                   lastOutput.size() > 200 ? lastOutput.substr(0, 200) +
  //                   "..."
  //                                           : lastOutput);
  //         }

  //         // Step 2: 评分
  //         TrainingScore score;
  //         if (trainCfg.scoringFunc) {
  //           score = co_await trainCfg.scoringFunc(lastOutput, testCase,
  //           iter);
  //         } else {
  //           score = co_await defaultScoringWithSubAgent(lastOutput, testCase,
  //                                                       iter, trainCfg);
  //         }
  //         score.iteration = iter;
  //         caseScores.push_back(score);

  //         if (trainCfg.verbose) {
  //           XX_LOGD("[Training] Case '{}' Iter {} Score: {:.3f}, Passed: {},
  //           "
  //                   "Feedback: {}",
  //                   testCase.name, iter + 1, score.score, score.passed,
  //                   score.feedback);
  //         }

  //         // 观察回调
  //         if (trainCfg.onIteration) {
  //           trainCfg.onIteration(score, lastOutput);
  //         }

  //         // Step 3: 检查是否收敛
  //         if (score.passed || score.score >= trainCfg.convergenceThreshold) {
  //           if (trainCfg.verbose) {
  //             XX_LOGD("[Training] Case '{}' converged at iteration {} with "
  //                     "score {:.3f}",
  //                     testCase.name, iter + 1, score.score);
  //           }
  //           break;
  //         }

  //         // Step 4: 调整处理（如果还有下一轮迭代）
  //         if (iter + 1 < trainCfg.maxIterations && trainCfg.adjustmentFunc) {
  //           extraMessages.clear();
  //           co_await trainCfg.adjustmentFunc(score, testCase,
  //           currentSystemPrompt,
  //                                            extraMessages);
  //           if (trainCfg.verbose) {
  //             XX_LOGD("[Training] Case '{}' Iter {} adjustment applied, "
  //                     "systemPrompt len={}, extraMessages count={}",
  //                     testCase.name, iter + 1, currentSystemPrompt.size(),
  //                     extraMessages.size());
  //           }
  //         }
  //       }

  //       allResults.push_back(std::move(caseScores));
  //     }

  //     // 打印汇总
  //     if (trainCfg.verbose) {
  //       size_t totalPassed = 0;
  //       for (size_t i = 0; i < allResults.size(); ++i) {
  //         const auto &scores = allResults[i];
  //         bool passed = false;
  //         if (false == scores.empty()) {
  //           passed = scores.back().passed;
  //         }
  //         if (passed)
  //           ++totalPassed;
  //         XX_LOGD("[Training] Case '{}': {} iterations, final score={:.3f}, "
  //                 "{}",
  //                 trainCfg.testCases[i].name, scores.size(),
  //                 scores.empty() ? 0.0 : scores.back().score,
  //                 passed ? "PASSED" : "FAILED");
  //       }
  //       XX_LOGD("[Training] Summary: {}/{} test cases passed", totalPassed,
  //               allResults.size());
  //     }

  //     co_return allResults;
  //   }

  //   /// 非阻塞启动训练模式
  //   void runTrainingModeNonBlocking(const TrainingConfig &trainCfg) {
  //     asio::co_spawn(
  //         *ioCtx,
  //         [this, trainCfg]() -> asio::awaitable<void> {
  //           co_await runTrainingMode(trainCfg);
  //         },
  //         asio::detached);
  //   }

  ~DeepAgent() { engine = nullptr; }
};
}; // namespace agentxx