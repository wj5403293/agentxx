#pragma once

#include "asio/io_context.hpp"
#include "fmt/base.h"
#include "fmt/format.h"
#include <functional>
#include <neograph/llm/rate_limited_provider.h>
#include <neograph/llm/schema_provider.h>
#include <neograph/neograph.h>
#include <string>
#include <string_view>
#include <vector>

namespace agentxx {
namespace tools {

class XXToolBase : public neograph::Tool {
protected:
  const std::string name;

public:
  const bool isDelayLoad;

  XXToolBase(const std::string &in_name, bool in_isDelayLoad)
      : name(in_name), isDelayLoad(in_isDelayLoad) {}

  std::string get_name() const override { return name; }
};

} // namespace tools
} // namespace agentxx