#pragma once

#include "log.h"
#include <agentxx.h>
#include <algorithm>
#include <boost/beast/core/detail/base64.hpp>
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

class AhoCorasick {
public:
  struct AhoCorasickMatchInfo {
    int start;
    int end;
    int patternId;

    AhoCorasickMatchInfo(int s, int e, int p)
        : start(s), end(e), patternId(p) {}

    bool operator<(const AhoCorasickMatchInfo &other) const {
      // 按起始位置升序，起始位置相同则按结束位置降序（长优先）
      if (start != other.start) {
        return start < other.start;
      }
      return end > other.end;
    }
  };

  AhoCorasick(std::vector<std::string_view> patterns,
              bool in_caseInsensitive = true)
      : caseInsensitive(in_caseInsensitive) {
    nodes.emplace_back(); // 初始化根节点
    for (auto &item : patterns) {
      addPattern(item);
    }
  }

  char onCharCode(char ch) const {
    if (caseInsensitive) {
      // 转换为小写字母
      if (ch >= 65 && ch <= 90) {
        return ch + 32;
      }
    }
    return ch;
  }

  // 添加模式串
  void addPattern(const std::string_view pattern) {
    patterns.emplace_back(pattern);
    int current = 0;
    for (char ch : pattern) {
      auto useChar = onCharCode(ch);
      auto idx = static_cast<unsigned char>(useChar);
      if (nodes[current].children[idx] == -1) {
        nodes[current].children[idx] = static_cast<int>(nodes.size());
        nodes.emplace_back();
      }
      current = nodes[current].children[idx];
    }
    nodes[current].outputs.push_back(static_cast<int>(patterns.size()) - 1);
  }

  const std::string &getPattern(int id) const { return patterns[id]; }

  // 构建失败指针和合并输出
  void build() {
    std::queue<int> q;
    nodes[0].fail = 0;

    // 初始化根节点的子节点
    for (int c = 0; c < 256; ++c) {
      if (nodes[0].children[c] != -1) {
        int child = nodes[0].children[c];
        nodes[child].fail = 0;
        q.push(child);
      }
    }

    // BFS构建失败指针
    while (!q.empty()) {
      int curr = q.front();
      q.pop();

      for (int c = 0; c < 256; ++c) {
        if (nodes[curr].children[c] == -1)
          continue;

        int child = nodes[curr].children[c];
        int fail = nodes[curr].fail;

        // 查找失败指针
        while (fail != 0 && nodes[fail].children[c] == -1) {
          fail = nodes[fail].fail;
        }

        nodes[child].fail =
            nodes[fail].children[c] != -1 ? nodes[fail].children[c] : 0;

        // 合并输出
        auto &outputs = nodes[child].outputs;
        auto &fail_outputs = nodes[nodes[child].fail].outputs;
        outputs.insert(outputs.end(), fail_outputs.begin(), fail_outputs.end());

        q.push(child);
      }
    }
  }

  // 搜索文本中的模式串
  std::vector<AhoCorasick::AhoCorasickMatchInfo>
  search(const std::string_view text, bool onlyContains = false) const {
    std::vector<AhoCorasick::AhoCorasickMatchInfo> matches;
    int current = 0;

    for (int i = 0; i < int(text.length()); ++i) {
      auto useChar = onCharCode(text[i]);
      auto c = static_cast<unsigned char>(useChar);

      // 跳转失败指针直到找到匹配或回到根节点
      while (current > 0 && nodes[current].children[c] == -1) {
        current = nodes[current].fail;
      }

      if (current >= 0) {
        current = nodes[current].children[c];
      }
      if (current < 0) {
        current = 0;
      }

      // 收集所有匹配的模式
      for (int patternId : nodes[current].outputs) {
        int start = i - static_cast<int>(patterns[patternId].length()) + 1;
        if (start >= 0) {
          // 验证有效的起始位置
          matches.emplace_back(start, i + 1, patternId);
          if (onlyContains) {
            // 只要存在一个，直接返回
            return matches;
          }
        }
      }
    }
    if (matches.empty()) {
      return matches;
    }

    std::sort(matches.begin(), matches.end());
    std::vector<AhoCorasick::AhoCorasickMatchInfo> result;
    int lastEnd = -1;
    for (const auto &match : matches) {
      if (match.start >= lastEnd) {
        // 新的非重叠匹配
        result.emplace_back(match);
        lastEnd = match.end;
      }
      // 如果match.start < lastEnd，说明这个匹配与之前的重叠
      // 由于我们已按结束位置降序排序，之前保留的是更长的匹配
      // 所以跳过这个较短的匹配
    }

    return matches;
  }

  std::string
  removeAll(std::string_view text,
            std::function<
                void(const std::vector<AhoCorasick::AhoCorasickMatchInfo> &)>
                onResults = nullptr) const {
    auto matches = search(text, false);

    if (nullptr != onResults) {
      onResults(matches);
    }

    if (matches.empty()) {
      return std::string{text};
    }

    size_t index = 0;
    auto result = std::string{};
    for (const auto &match : matches) {
      if (match.start > int(index)) {
        result += text.substr(index, match.start - int(index));
      }
      size_t end = match.end;
      if (end > index) {
        index = end;
      }
    }
    if (index < text.length()) {
      result += text.substr(index);
    }

    return result;
  }

private:
  struct Node {
    int children[256];        // ASCII字符集
    int fail;                 // 失败指针
    std::vector<int> outputs; // 输出模式索引

    Node() {
      memset(children, -1, sizeof(children));
      fail = -1;
    }
  };

  bool caseInsensitive;
  std::vector<Node> nodes{};           // Trie节点存储
  std::vector<std::string> patterns{}; // 所有模式串
};

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

inline std::string_view stringCopyMalloc(const std::string_view data1,
                                         const std::string_view data2 = "",
                                         const std::string_view data3 = "",
                                         const std::string_view data4 = "") {
  const auto len = data1.size() + data2.size() + data3.size() + data4.size();
  auto size = len + 1;
  auto result = (char *)agentxx_malloc(size);
  memcpy(result, data1.data(), data1.size());
  if (false == data2.empty()) {
    memcpy(result + data1.size(), data2.data(), data2.size());
    if (false == data3.empty()) {
      memcpy(result + data1.size() + data2.size(), data3.data(), data3.size());
      if (false == data4.empty()) {
        memcpy(result + data1.size() + data2.size() + data3.size(),
               data4.data(), data4.size());
      }
    }
  }
  result[len] = '\0';
  return std::string_view{result, len};
}

inline static std::string base64_encode(const void *data, size_t len) {
  // 预分配
  std::string result;
  result.resize(boost::beast::detail::base64::encoded_size(len));

  size_t bytes_written =
      boost::beast::detail::base64::encode(result.data(), data, len);

  // 调整大小为实际写入的字节数
  result.resize(bytes_written);
  return result;
}
}; // namespace util
}; // namespace agentxx