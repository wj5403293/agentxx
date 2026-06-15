#include "agentxx.h"
#include "analyse/audio_visualization.h"
#include "analyse/codec_info.h"
#include "analyse/image.h"
#include "analyse/media_info.h"
#include "simdjson.h"
#include "util/log.h"
#include "util/string_util.h"
#include "util/util.h"
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

using namespace agentxx;

FFI_PLUGIN_EXPORT void *agentxx_malloc(size_t size) { return malloc(size); }

FFI_PLUGIN_EXPORT void agentxx_free(const void *ptr) {
  XX_LOGD("agentxx_free : {}", ptr);
  // 如果此处出错，也可能是在此之前 ptr 已经越界访问，释放时 debug
  // 检查出存在越界写入
  free(const_cast<void *>(ptr));
}
