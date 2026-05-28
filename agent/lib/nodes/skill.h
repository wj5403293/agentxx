#pragma once

#include "asio/io_context.hpp"
#include <cstdlib>
#include <functional>
#include <iostream>
#include <neograph/llm/rate_limited_provider.h>
#include <neograph/llm/schema_provider.h>
#include <neograph/neograph.h>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace agentxx {
namespace nodes {

class NEOGRAPH_API SkillNode : public neograph::graph::GraphNode {
protected:
public:
  inline static const auto defNodeType = std::string_view{"xx_skill_loader"};

  SkillNode(const neograph::graph::NodeContext &ctx) {}

  asio::awaitable<neograph::graph::NodeOutput>
  run(neograph::graph::NodeInput in) override {}

  std::string get_name() const override { return "SkillLoader"; }
};

} // namespace nodes
} // namespace agentxx