#pragma once

#include "fmt/format.h"
#include "utilxx.h"
#include <iostream>
#include <string_view>

#ifdef LUMENXX_BUILD_TYPE

#define XX_LOGD(str, ...)                                                      \
  (std::cerr << fmt::format(str, ##__VA_ARGS__) << std::endl);

#define XX_LOGI(str, ...)                                                      \
  (std::cerr << fmt::format(str, ##__VA_ARGS__) << std::endl);

#define XX_LOGW(str, ...)                                                      \
  (std::cerr << fmt::format(str, ##__VA_ARGS__) << std::endl);

#define XX_LOGE(str, ...)                                                      \
  (std::cerr << fmt::format(str, ##__VA_ARGS__) << std::endl);

#else

#define XX_LOGD(str, ...) ;

#define XX_LOGI(str, ...) ;

#define XX_LOGW(str, ...) ;

#define XX_LOGE(str, ...) ;

#endif

namespace agentxx {
namespace logxx {
void printStack();

void signalError(std::string_view exepath);
}; // namespace logxx
}; // namespace agentxx