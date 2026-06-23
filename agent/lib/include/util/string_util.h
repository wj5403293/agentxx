#pragma once

#include "boost/beast/core/detail/base64.hpp"
#include "log.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <queue>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace agentxx {
namespace util {

// 不区分大小写哈希
struct IgnoreCaseHash {
  size_t operator()(const std::string &s) const {
    std::string lower_s;
    lower_s.reserve(s.size());
    for (char c : s) {
      lower_s.push_back(tolower(static_cast<unsigned char>(c)));
    }
    return std::hash<std::string>()(lower_s);
  }
};

// 不区分大小写相等判断（用于无序容器）
struct IgnoreCaseEqual {
  bool operator()(const std::string &a, const std::string &b) const {
    if (a.size() != b.size()) {
      return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
      if (tolower(static_cast<unsigned char>(a[i])) !=
          tolower(static_cast<unsigned char>(b[i]))) {
        return false;
      }
    }
    return true;
  }
};

template <typename V>
using IgnoreCaseMap =
    std::unordered_map<std::string, V, IgnoreCaseHash, IgnoreCaseEqual>;

using IgnoreCaseSet =
    std::unordered_set<std::string, IgnoreCaseHash, IgnoreCaseEqual>;

inline size_t utf8GetLength(std::string_view in_str) {
  size_t length = 0;
  for (size_t i = 0, step = 0; i < in_str.size(); i += step) {
    unsigned char byte = in_str[i];
    // lenght 6
    if (byte >= 0xFC) {
      step = 6;
    } else if (byte >= 0xF8) {
      step = 5;
    } else if (byte >= 0xF0) {
      step = 4;
    } else if (byte >= 0xE0) {
      step = 3;
    } else if (byte >= 0xC0) {
      step = 2;
    } else {
      step = 1;
    }
    length++;
  }
  return length;
}

inline size_t findIndexByUtf8Length(std::string_view in_str, size_t targetLen,
                                    size_t start = 0) {
  size_t count = 0;
  size_t i = start, step = 0;
  for (; i < in_str.size();) {
    unsigned char byte = in_str[i];
    // lenght 6
    if (byte >= 0xFC) {
      step = 6;
    } else if (byte >= 0xF8) {
      step = 5;
    } else if (byte >= 0xF0) {
      step = 4;
    } else if (byte >= 0xE0) {
      step = 3;
    } else if (byte >= 0xC0) {
      step = 2;
    } else {
      step = 1;
    }
    ++count;
    i += step;

    if (count >= targetLen) {
      return i;
    }
  }
  // length not found
  return 0;
}

/// return <index, lineCount, lastLineIndex>
inline std::tuple<size_t, size_t, size_t>
findIndexAndLastLineIndexByUtf8Length(std::string_view in_str,
                                      size_t targetLen) {
  if (in_str.size() >= targetLen) {
    size_t count = 0, lineCount = 0, lastLineIndex = 0;
    size_t i = 0, step = 0;
    for (; i < in_str.size();) {
      const unsigned char byte = in_str[i];
      // lenght 6
      if (byte >= 0xFC) {
        step = 6;
      } else if (byte >= 0xF8) {
        step = 5;
      } else if (byte >= 0xF0) {
        step = 4;
      } else if (byte >= 0xE0) {
        step = 3;
      } else if (byte >= 0xC0) {
        step = 2;
      } else {
        step = 1;
        if (byte == (unsigned char)'\n') {
          ++lineCount;
          lastLineIndex = i;
        }
      }
      ++count;
      i += step;

      if (count >= targetLen) {
        return std::tuple<size_t, size_t, size_t>{i, lineCount, lastLineIndex};
      }
    }
  }
  // length not found
  return std::tuple<size_t, size_t, size_t>{0, 0, 0};
}

inline bool utf8IsContinuationChar(unsigned char ch) {
  return (ch & 0xC0) == 0x80; // 10xxxxxx 的二进制特征：前两位是 10
}

inline size_t utf8GetLengthCheckAvail(std::string_view str) {
  size_t length = 0;
  const auto strLen = str.length();
  for (size_t i = 0, step = 0; i < strLen; i += step) {
    unsigned char ch = str[i];
    if (ch == '\0') {
      break;
    } else if (ch >= 0xF8) {
      // (ch >= 0xFC || ch >= 0xF8)
      // lenght 6、5 无效
      return 0;
    } else if (ch >= 0xF0) {
      if (ch == 0xF0 &&
          (static_cast<unsigned char>(str[i + 1]) & 0xF0) == 0x80) {
        // 检查非最短编码长度，对应 0~0xFFFF
        return 0;
      }
      step = 4;
    } else if (ch >= 0xE0) {
      if (ch == 0xE0 &&
          (static_cast<unsigned char>(str[i + 1]) & 0xE0) == 0x80) {
        // 0xE0 0x80~0x9F 对应 0~0x7FF
        return 0;
      }
      if (ch == 0xEF && i + 2 < strLen) {
        unsigned char ch1 = static_cast<unsigned char>(str[i + 1]);
        unsigned char ch2 = static_cast<unsigned char>(str[i + 2]);
        if (ch1 == 0xBF && ch2 == 0xBD) {
          return 0; // 匹配�，判定为无效UTF-8
        }
      }
      step = 3;
    } else if (ch >= 0xC0) {
      if (ch == 0xC0 || ch == 0xC1) {
        // 对应 0~0x3F
        return 0;
      }
      step = 2;
    } else {
      if (str[i] < 0) {
        return 0;
      }
      step = 1;
    }

    if (step > 1) {
      for (size_t j = 1; j < step; j++) {
        if (i + j >= strLen || str[i + j] == '\0' ||
            false == utf8IsContinuationChar((unsigned char)str[i + j])) {
          // 不合规
          return 0;
        }
      }
    }
    length++;
  }
  return length;
}

inline bool utf8IsAvail(std::string_view str) {
  if (str.empty() || str[0] == '\0' || (utf8GetLengthCheckAvail(str) == 0)) {
    return false;
  }
  return true;
}

template <typename T>
inline std::string stringVectorJoin(const std::vector<T> &list,
                                    const std::string_view sep = ", ") {
  std::ostringstream oss;
  auto len = list.size();
  for (size_t i = 0; i < len; ++i) {
    oss << list[i];
    if (i < len - 1) {
      oss << sep;
    }
  }
  return oss.str();
}

inline std::vector<std::string> strSplit(const std::string &in_str,
                                         char in_char) {
  std::vector<std::string> re_strlist{};
  std::istringstream iss{in_str}; // 输入流
  for (std::string item{};
       std::getline(iss, item, in_char);) { // 以split为分隔符
    re_strlist.push_back(item);
  }
  return re_strlist;
}

inline std::string_view strTrim(std::string_view sv) {
  while (!sv.empty() && std::isspace(sv.front())) {
    sv.remove_prefix(1);
  }
  while (!sv.empty() && std::isspace(sv.back())) {
    sv.remove_suffix(1);
  }
  return sv;
}

inline void printStringToIntList(const char *str) {
  if (nullptr == str) {
    XX_LOGI("[]");
    return;
  }
  std::cout << "[";
  for (int i = 0;; ++i) {
    if (str[i] == '\0') {
      break;
    }
    if (i > 0) {
      std::cout << ", ";
    }
    std::cout << int(str[i]);
  }
  std::cout << "]" << std::endl;
}

inline std::string_view toStringNotNull(const char *str) {
  if (nullptr == str) {
    return std::string_view{""};
  }
  return std::string_view{str};
}

inline std::string base64_encode(const void *data, size_t len) {
  // 预分配
  std::string result;
  result.resize(boost::beast::detail::base64::encoded_size(len));

  size_t bytes_written =
      boost::beast::detail::base64::encode(result.data(), data, len);

  // 调整大小为实际写入的字节数
  result.resize(bytes_written);
  return result;
}

inline std::string base64_decode(const std::string_view str) {
  std::string result;
  result.resize(boost::beast::detail::base64::decoded_size(str.size()));

  auto [bytes_written, _] = boost::beast::detail::base64::decode(
      result.data(), str.data(), str.size());

  result.resize(bytes_written);
  return result;
}

std::string convertToUtf8(const std::string_view src, const char *src_encoding);
bool chardetConvertEncoding(const std::string_view str, std::string &encoding,
                            std::string &result);
}; // namespace util
}; // namespace agentxx