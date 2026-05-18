#include "string_util.h"
#include <algorithm>
#include <cstring>
#include <iconv.h>
#include <set>
#include <vector>

// BOM 判断UTF16编码
static std::string detectUtfBom(const std::string_view str) {
  if (str.size() < 2) {
    // UTF16 BOM至少2字节，长度不足直接返回
    return "";
  }
  const unsigned char *bom =
      reinterpret_cast<const unsigned char *>(str.data());
  if (bom[0] == 0xFF && bom[1] == 0xFE) {
    // UTF-16LE BOM (0xFF 0xFE)
    return "UTF-16LE";
  } else if (bom[0] == 0xFE && bom[1] == 0xFF) {
    // UTF-16BE BOM (0xFE 0xFF)
    return "UTF-16BE";
  } else if (str.size() >= 3 && bom[0] == 0xEF && bom[1] == 0xBB &&
             bom[2] == 0xBF) {
    return "UTF-8";
  }
  // 无BOM，返回空
  return "";
}
