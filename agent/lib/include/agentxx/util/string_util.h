#pragma once

#include "agentxx/util/log.h"
#include "boost/beast/core/detail/base64.hpp"
#include <algorithm>
#include <cstring>
#include <functional>
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

static constexpr int CODE_0 = 48;
static constexpr int CODE_9 = 57;
static constexpr int CODE_A = 65;
static constexpr int CODE_Z = 90;
static constexpr int CODE_a = 97;
static constexpr int CODE_z = 122;

using PinyinCallback = std::function<std::string(const std::string &)>;

inline bool isCode_num(int code) { return (code >= CODE_0 && code <= CODE_9); }

inline bool isCode_AZ(int code) { return (code >= CODE_A && code <= CODE_Z); }

inline bool isCode_az(int code) { return (code >= CODE_a && code <= CODE_z); }

inline bool isCode_AZaz(int code) {
  return (isCode_AZ(code) || isCode_az(code));
}

inline std::optional<int> toCode_tryAZ(int code) {
  if (isCode_az(code)) {
    return code - (CODE_a - CODE_A);
  } else if (isCode_AZ(code)) {
    return code;
  }
  return std::nullopt;
}

inline std::optional<int> toCode_tryaz(int code) {
  if (isCode_az(code)) {
    return code;
  } else if (isCode_AZ(code)) {
    return code + (CODE_a - CODE_A);
  }
  return std::nullopt;
}

inline int toCode_mayAZ(int code) {
  auto result = toCode_tryAZ(code);
  return result.has_value() ? result.value() : code;
}

inline int toCode_mayaz(int code) {
  auto result = toCode_tryaz(code);
  return result.has_value() ? result.value() : code;
}

inline int toCode_AZ(int code) {
  if (isCode_az(code)) {
    return code - (CODE_a - CODE_A);
  }
  assert(isCode_AZ(code));
  return code;
}

inline int toCode_az(int code) {
  if (isCode_AZ(code)) {
    return code + (CODE_a - CODE_A);
  }
  assert(isCode_az(code));
  return code;
}

inline void toLowerSelf(std::string &str) {
  std::transform(str.begin(), str.end(), str.begin(),
                 [](unsigned char c) { return std::tolower(c); });
}

inline std::string toLower(const std::string &str) {
  std::string result = str;
  toLowerSelf(result);
  return result;
}

// 不区分大小写哈希
struct IgnoreCaseHash {
  size_t operator()(const std::string &s) const {
    return std::hash<std::string>()(toLower(s));
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

inline std::string base64Encode(const void *data, size_t len) {
  // 预分配
  std::string result;
  result.resize(boost::beast::detail::base64::encoded_size(len));

  size_t bytes_written =
      boost::beast::detail::base64::encode(result.data(), data, len);

  // 调整大小为实际写入的字节数
  result.resize(bytes_written);
  return result;
}

inline std::string base64Decode(const std::string_view str) {
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

inline PinyinCallback s_pinyinCallback = nullptr;

inline std::string removeAllSpace(const std::string &str) {
  std::string result;
  result.reserve(str.size());
  for (char c : str) {
    if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
      result += c;
    }
  }
  return result;
}

inline std::optional<std::string>
removeAllSpaceMayNull(const std::string &str) {
  if (str.empty()) {
    return std::nullopt;
  }
  std::string result = removeAllSpace(str);
  if (result.empty()) {
    return std::nullopt;
  }
  return result;
}

inline std::string removeBetweenSpace(const std::string &str,
                                      bool removeLine = true,
                                      bool subLeft = true,
                                      bool subRight = true) {

  assert(subLeft || subRight);

  if (str.empty()) {
    return str;
  }

  int left = 0;
  int right = static_cast<int>(str.size()) - 1;

  if (subRight) {
    for (; right >= left; --right) {
      if (str[right] != ' ' && str[right] != '\t' &&
          (!removeLine || (str[right] != '\r' && str[right] != '\n'))) {
        break;
      }
    }
  }

  if (subLeft) {
    for (; left <= right; ++left) {
      if (str[left] != ' ' && str[left] != '\t' &&
          (!removeLine || (str[left] != '\r' && str[left] != '\n'))) {
        break;
      }
    }
  }

  if (left <= right) {
    return str.substr(left, right - left + 1);
  } else {
    return "";
  }
}

inline std::optional<std::string>
removeBetweenSpaceMayNull(const std::string &str, bool removeLine = true,
                          bool subLeft = true, bool subRight = true) {

  if (str.empty()) {
    return std::nullopt;
  }

  std::string result = removeBetweenSpace(str, removeLine, subLeft, subRight);
  if (result.empty()) {
    return std::nullopt;
  }
  return result;
}

inline std::string getFirstWordPinyin(const std::string &str) {
  if (s_pinyinCallback) {
    return s_pinyinCallback(str);
  }
  return "";
}

inline std::string getFirstCharPinyinFast(const std::string &str) {
  if (str.empty())
    return "";

  int code = static_cast<unsigned char>(str[0]);
  if (isCode_num(code))
    return std::string(1, str[0]);
  if (isCode_az(code))
    return std::string(1, str[0]);
  if (isCode_AZ(code))
    return std::string(1, static_cast<char>(code + (CODE_a - CODE_A)));

  std::string result = getFirstWordPinyin(str);
  if (!result.empty()) {
    int firstCode = static_cast<unsigned char>(result[0]);
    if (isCode_AZaz(firstCode)) {
      toLowerSelf(result);
      return result;
    }
  }
  return "";
}

inline std::optional<int> getComparableCode(const std::string &str,
                                            size_t index) {
  int code = static_cast<unsigned char>(str[index]);

  if (isCode_az(code))
    return code;
  if (isCode_AZ(code))
    return code + (CODE_a - CODE_A);

  if (code < 128)
    return std::nullopt;

  std::string target = (index == 0) ? str : str.substr(index);
  std::string pinyin = getFirstCharPinyinFast(target);
  if (!pinyin.empty()) {
    int pinyinCode = static_cast<unsigned char>(pinyin[0]);
    if (isCode_AZ(pinyinCode))
      return pinyinCode + (CODE_a - CODE_A);
    if (isCode_az(pinyinCode))
      return pinyinCode;
  }
  return std::nullopt;
}

inline std::optional<std::string> getFirstCharPinyin(const std::string &str,
                                                     bool enableAZ = true,
                                                     bool enableNum = true) {
  if (str.empty()) {
    return std::nullopt;
  }

  std::string trimmed = removeBetweenSpace(str, true, true, false);
  if (trimmed.empty()) {
    return std::nullopt;
  }

  int code = static_cast<unsigned char>(trimmed[0]);

  if (isCode_num(code)) {
    if (enableNum) {
      return std::string(1, trimmed[0]);
    } else {
      return std::nullopt;
    }
  } else if (isCode_az(code)) {
    if (enableAZ) {
      return std::string(1, trimmed[0]);
    } else {
      return std::nullopt;
    }
  } else if (isCode_AZ(code)) {
    if (enableAZ) {
      return std::string(1, static_cast<char>(code + (CODE_a - CODE_A)));
    } else {
      return std::nullopt;
    }
  } else {
    std::string result = getFirstWordPinyin(trimmed);
    if (!result.empty()) {
      int firstCode = static_cast<unsigned char>(result[0]);
      if (isCode_AZaz(firstCode)) {
        toLowerSelf(result);
        return result;
      }
    }
    return std::nullopt;
  }
}

inline std::optional<std::string>
getFirstCharPinyinFirstChar(const std::string &str) {
  auto restr = getFirstCharPinyin(str);
  if (restr.has_value() && !restr->empty()) {
    return std::string(1, (*restr)[0]);
  }
  return std::nullopt;
}

inline int compareExtend(const std::string &left, const std::string &right) {
  if (left.empty()) {
    if (right.empty()) {
      return 0;
    }
    return -1;
  }
  if (right.empty()) {
    return 1;
  }

  size_t i = 0, j = 0;

  while (i < left.size() && j < right.size()) {
    int leftCode = static_cast<unsigned char>(left[i]);
    int rightCode = static_cast<unsigned char>(right[j]);
    bool leftIsNum = isCode_num(leftCode);
    bool rightIsNum = isCode_num(rightCode);

    if (leftIsNum != rightIsNum) {
      return leftIsNum ? -1 : 1;
    }

    if (leftIsNum) {
      int leftSum = 0;
      while (i < left.size() &&
             isCode_num(static_cast<unsigned char>(left[i]))) {
        leftSum = leftSum * 10 + (static_cast<unsigned char>(left[i]) - CODE_0);
        i++;
      }
      int rightSum = 0;
      while (j < right.size() &&
             isCode_num(static_cast<unsigned char>(right[j]))) {
        rightSum =
            rightSum * 10 + (static_cast<unsigned char>(right[j]) - CODE_0);
        j++;
      }
      if (leftSum != rightSum) {
        return leftSum - rightSum;
      }
      continue;
    }

    auto leftComp = getComparableCode(left, i);
    auto rightComp = getComparableCode(right, j);

    if (leftComp.has_value() && rightComp.has_value()) {
      int result = leftComp.value() - rightComp.value();
      if (result != 0)
        return result;
    } else if (leftComp.has_value()) {
      return 1;
    } else if (rightComp.has_value()) {
      return -1;
    } else {
      int leftLower = toCode_tryaz(leftCode).value_or(leftCode);
      int rightLower = toCode_tryaz(rightCode).value_or(rightCode);
      int result = leftLower - rightLower;
      if (result != 0)
        return result;
    }

    i++;
    j++;
  }

  if (i < left.size() || j < right.size()) {
    return static_cast<int>(left.size() - right.size());
  }
  return 0;
}

inline std::string collapseSlashes(const std::string &path) {
  std::string result;
  result.reserve(path.size());
  bool prevSlash = false;
  for (char c : path) {
    if (c == '/') {
      if (!prevSlash) {
        result += c;
      }
      prevSlash = true;
    } else {
      result += c;
      prevSlash = false;
    }
  }
  return result;
}

inline std::string collapseBackslashes(const std::string &path) {
  std::string result;
  result.reserve(path.size());
  bool prevBackslash = false;
  for (char c : path) {
    if (c == '\\') {
      if (!prevBackslash) {
        result += c;
      }
      prevBackslash = true;
    } else {
      result += c;
      prevBackslash = false;
    }
  }
  return result;
}

inline std::string collapseMixedSlashes(const std::string &path) {
  std::string result;
  result.reserve(path.size());
  size_t i = 0;
  while (i < path.size()) {
    if (path[i] == '/' || path[i] == '\\') {
      size_t start = i;
      while (i < path.size() && (path[i] == '/' || path[i] == '\\')) {
        i++;
      }
      size_t count = i - start;
      if (count >= 2) {
        result += '\\';
      } else {
        result += path[start];
      }
    } else {
      result += path[i];
      i++;
    }
  }
  return result;
}

inline std::string toStandardPath(const std::string &path) {
  std::string result = collapseSlashes(path);
  result = collapseBackslashes(result);
  result = collapseMixedSlashes(result);
  return result;
}

inline std::string toWindowsStandardPath(const std::string &path) {
  std::string result;
  result.reserve(path.size());
  bool prevSlash = false;
  for (char c : path) {
    if (c == '/' || c == '\\') {
      if (!prevSlash) {
        result += '\\';
      }
      prevSlash = true;
    } else {
      result += c;
      prevSlash = false;
    }
  }
  return result;
}

inline std::string toUnixStandardPath(const std::string &path) {
  std::string result;
  result.reserve(path.size());
  bool prevSlash = false;
  for (char c : path) {
    if (c == '/' || c == '\\') {
      if (!prevSlash) {
        result += '/';
      }
      prevSlash = true;
    } else {
      result += c;
      prevSlash = false;
    }
  }
  return result;
}

inline std::string toUnixStandardDirPath(const std::string &path) {
  if (path.empty()) {
    return path;
  }
  std::string normalized = toUnixStandardPath(path);
  if (normalized.back() == '/') {
    return normalized;
  }
  return normalized + "/";
}

inline std::string getFileName(const std::string &in_path,
                               bool removeEXT = false,
                               bool useRigthDot = true) {

  if (in_path.empty()) {
    return "";
  }

  int i = static_cast<int>(in_path.size());
  bool isContinueDot = false;

  while (i-- > 0) {
    if (in_path[i] != '/' && in_path[i] != '\\') {
      break;
    }
    removeEXT = false;
  }

  int nameEndIndex = i + 1;

  int leftDotIndex = -1;
  int leftTempDotIndex = -1;
  int rightDotIndex = -1;

  if (nameEndIndex <= 0) {
    return in_path;
  }

  for (; i-- > 0;) {
    if (in_path[i] == '/' || in_path[i] == '\\') {
      if (i == static_cast<int>(in_path.size())) {
        return "";
      } else {
        break;
      }
    } else if ('.' == in_path[i]) {
      if (removeEXT) {
        if (rightDotIndex == -1) {
          rightDotIndex = i;
        }
        leftTempDotIndex = leftDotIndex;
        leftDotIndex = i;
        isContinueDot = (leftDotIndex != rightDotIndex);
      }
    } else {
      isContinueDot = false;
    }
  }

  if (rightDotIndex == -1) {
    rightDotIndex = nameEndIndex;
  }

  int start = i + 1;
  int end = -1;

  if (useRigthDot) {
    end = rightDotIndex;
  } else {
    end = leftDotIndex;
  }

  if (start < 0) {
    start = 0;
  }
  if (end < 0) {
    end = 0;
  }

  if (leftDotIndex == start) {
    if (isContinueDot || false == removeEXT || leftDotIndex == rightDotIndex) {
      end = nameEndIndex;
    } else {
      if (useRigthDot) {
        end = rightDotIndex;
      } else {
        end = leftTempDotIndex;
      }
    }
  }

  return in_path.substr(start, end - start);
}

inline std::optional<std::string> getFileNameEXT(const std::string &in_path) {
  if (in_path.empty() || in_path.back() == '.' || in_path.back() == '/' ||
      in_path.back() == '\\') {
    return std::nullopt;
  }

  for (int i = static_cast<int>(in_path.size()) - 1; i-- > 1;) {
    if (in_path[i] == '.') {
      return in_path.substr(i + 1);
    }
  }
  return std::nullopt;
}

inline std::string replaceOrAppendExt(const std::string &inpath,
                                      const std::string &newExt) {
  auto ext = getFileNameEXT(inpath);
  if (ext.has_value()) {
    std::string result = inpath;
    result.replace(inpath.size() - ext->size(), ext->size(), newExt);
    return result;
  }
  if (!inpath.empty() && inpath.back() == '.') {
    return inpath + newExt;
  }
  return inpath + "." + newExt;
}

inline std::optional<std::string> getParentDirPath(const std::string &in_path) {
  if (in_path.empty()) {
    return std::nullopt;
  }

  int i = static_cast<int>(in_path.size());

  while (i-- > 0) {
    if (in_path[i] != '/' && in_path[i] != '\\') {
      break;
    }
  }

  if (i < 0 && !in_path.empty()) {
    return "/";
  }

  for (; i-- > 0;) {
    if (in_path[i] == '/' || in_path[i] == '\\') {
      if (i == static_cast<int>(in_path.size())) {
        return "";
      } else {
        return in_path.substr(0, i + 1);
      }
    }
  }

  return std::nullopt;
}

inline bool isIgnoreCaseEqual(const std::string &left,
                              const std::string &right) {
  if (left.size() == right.size()) {
    return toLower(left) == toLower(right);
  }
  return false;
}

inline bool isIgnoreCaseContains(const std::string &longStr,
                                 const std::string &shortStr) {
  std::string lowerLong = toLower(longStr);
  std::string lowerShort = toLower(shortStr);
  return lowerLong.find(lowerShort) != std::string::npos;
}

inline bool isIgnoreCaseContainsAny(const std::string &str1,
                                    const std::string &str2) {
  return (str1.size() >= str2.size()) ? isIgnoreCaseContains(str1, str2)
                                      : isIgnoreCaseContains(str2, str1);
}

inline bool isNotEmptyAndIgnoreCaseContains(const std::string &str1,
                                            const std::string &str2) {
  if (str1.empty() || str2.empty()) {
    return false;
  }
  return isIgnoreCaseContains(str1, str2);
}

inline bool isNotEmptyAndIgnoreCaseContainsAny(const std::string &str1,
                                               const std::string &str2) {
  if (str1.empty() || str2.empty()) {
    return false;
  }
  return isIgnoreCaseContainsAny(str1, str2);
}

inline std::optional<std::string>
subString(const std::string &str, size_t start = 0,
          std::optional<size_t> end = std::nullopt) {

  if (start > str.size()) {
    return std::nullopt;
  }

  size_t actualEnd = str.size();
  if (end.has_value()) {
    if (end.value() < start) {
      return std::nullopt;
    }
    if (end.value() > str.size()) {
      actualEnd = str.size();
    } else {
      actualEnd = end.value();
    }
  }

  return str.substr(start, actualEnd - start);
}

inline std::string toArgument(const std::string &str, char mark = '"') {
  std::string result;
  result.reserve(str.size() * 2);
  size_t len = str.size();

  for (size_t i = 0; i < len; ++i) {
    if (str[i] == mark) {
      bool shouldEscape = false;

      if (i == 0) {
        shouldEscape = true;
      } else {
        char prev = str[i - 1];
        if (prev != '\\') {
          shouldEscape = true;
        }
      }

      if (shouldEscape) {
        result += '\\';
      }
      result += mark;
    } else {
      result += str[i];
    }
  }

  return std::string(1, mark) + result + std::string(1, mark);
}

}; // namespace util
}; // namespace agentxx