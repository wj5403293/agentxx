#pragma once

#include "tools/tool.h"
#include "util/http_client.h"
#include "util/string_util.h"
#include <expected>
#include <filesystem>
#include <format>
#include <iostream>
#include <map>
#include <memory>
#include <neograph/llm/rate_limited_provider.h>
#include <neograph/llm/schema_provider.h>
#include <neograph/neograph.h>
#include <sstream>
#include <string>
#include <vector>

namespace agentxx {
namespace tools {

class EmbeddingClient {
public:
  EmbeddingClient(const std::string &in_baseUrl, const std::string &in_apiKey,
                  const std::string in_model)
      : baseUrl(in_baseUrl), apiKey(in_apiKey), model(in_model) {}

  inline static std::vector<std::string>
  splitStringByFixedLength(const std::string_view text,
                           size_t blockSize = 256) {
    auto result = std::vector<std::string>{};
    for (size_t index = 0; index < text.size();) {
      auto target =
          agentxx::util::findIndexByUtf8Length(text, blockSize, index);
      if (target <= 0) {
        target = text.size();
      }
      result.push_back(std::string{text.substr(index, target - index)});
      index = target;
    }
    return result;
  }

  // Embed multiple texts in one API call
  asio::awaitable<std::expected<std::vector<std::vector<float>>, std::string>>
  embed_batch(const std::vector<std::string> &texts) const {
    if (texts.empty()) {
      co_return std::expected<std::vector<std::vector<float>>, std::string>{
          std::vector<std::vector<float>>{}};
    }

    auto body = neograph::json::object();
    body["model"] = model;
    body["input"] = neograph::json(texts);

    auto resp = co_await agentxx::util::HttpClient::postAsync(
        fmt::format("{}/v1/embeddings", baseUrl), body,
        std::chrono::seconds{15});

    if (false == resp.has_value() ||
        false == agentxx::util::HttpClient::respIsSucc(resp.value())) {
      std::string str;
      if (resp.has_value()) {
        str = std::to_string(resp.value()->status);
      } else {
        str = resp.error();
      }

      co_return std::unexpected{fmt::format("[embedding] API error: {}", str)};
    }

    auto respBody = neograph::json::parse(resp.value()->body);
    std::vector<std::vector<float>> embeddings;

    for (const auto &item : respBody["data"]) {
      std::vector<float> vec;
      for (const auto &v : item["embedding"]) {
        vec.push_back(v.get<float>());
      }
      embeddings.push_back(std::move(vec));
    }

    co_return std::expected<std::vector<std::vector<float>>, std::string>{
        embeddings};
  }

private:
  std::string baseUrl;
  std::string apiKey;
  std::string model;
};

class RAGSearchTool : public XXToolBase {
public:
  // =========================================================================
  // Vector Store — in-memory with cosine similarity search
  // =========================================================================
  struct Document {
    std::string id;
    std::string title;
    std::vector<std::string> content;
    std::string source;
    std::vector<std::vector<float>> embedding;
  };

  static double cosine_similarity(const std::vector<float> &a,
                                  const std::vector<float> &b) {
    if (a.size() != b.size() || a.empty())
      return 0.0;
    double dot = 0.0, norm_a = 0.0, norm_b = 0.0;
    for (size_t i = 0; i < a.size(); ++i) {
      dot += a[i] * b[i];
      norm_a += a[i] * a[i];
      norm_b += b[i] * b[i];
    }
    double denom = std::sqrt(norm_a) * std::sqrt(norm_b);
    return denom > 0.0 ? dot / denom : 0.0;
  }

  class VectorStore {
  protected:
    std::shared_ptr<EmbeddingClient> embedder;
    std::vector<Document> docs;

  public:
    VectorStore(std::shared_ptr<EmbeddingClient> in_embedder)
        : embedder(std::move(in_embedder)) {}

    // Add documents and compute their embeddings
    asio::awaitable<bool> addDocuments(std::vector<Document> &&appendDocs) {
      if (appendDocs.empty()) {
        co_return true;
      }
      // Batch embed all documents at once
      std::vector<std::string> texts;
      for (auto &doc : appendDocs) {
        texts.insert(texts.end(), doc.content.begin(), doc.content.end());
      }

      auto embeddings = co_await embedder->embed_batch(texts);

      if (embeddings.has_value()) {
        auto start = embeddings.value().begin();
        for (size_t i = 0; i < appendDocs.size(); ++i) {
          appendDocs[i].embedding = std::vector<std::vector<float>>{
              start, start + appendDocs[i].content.size()};
          start += appendDocs[i].content.size();
          docs.push_back(std::move(appendDocs[i]));
        }
        co_return true;
      }
      co_return false;
    }

    // Search by cosine similarity
    asio::awaitable<std::expected<
        std::vector<std::tuple<Document, size_t, double>>, std::string>>
    search(const std::string &query, size_t top_k = 3) const {
      auto queryVec = co_await embedder->embed_batch(
          EmbeddingClient::splitStringByFixedLength(query));
      if (false == queryVec.has_value()) {
        co_return std::unexpected{queryVec.error()};
      }
      if (queryVec.value().empty()) {
        co_return std::expected<
            std::vector<std::tuple<Document, size_t, double>>, std::string>{};
      }

      /// <docIndex, contentIndex, sim>
      std::vector<std::tuple<size_t, size_t, double>> scores;
      for (size_t i = 0; i < docs.size(); ++i) {
        if (!docs[i].embedding.empty()) {
          for (size_t j = 0; j < docs[i].embedding.size(); ++j) {
            double sim = 0;
            for (const auto &queryVecItem : queryVec.value()) {
              sim += cosine_similarity(queryVecItem, docs[i].embedding[j]);
            }
            scores.push_back({i, j, sim / queryVec->size()});
          }
        }
      }

      std::sort(scores.begin(), scores.end(),
                [](const std::tuple<size_t, size_t, double> &a,
                   const std::tuple<size_t, size_t, double> &b) {
                  const auto &[_a1, _a2, aSim] = a;
                  const auto &[_b1, _b2, bSim] = b;
                  return aSim > bSim;
                });

      std::vector<std::tuple<Document, size_t, double>> results;
      for (size_t i = 0; i < top_k && i < scores.size(); ++i) {
        const auto [docIndex, contentIndex, sim] = scores[i];
        results.push_back({docs[docIndex], contentIndex, sim});
      }
      co_return std::expected<std::vector<std::tuple<Document, size_t, double>>,
                              std::string>{results};
    }
  };

  std::shared_ptr<VectorStore> store;

  explicit RAGSearchTool(std::shared_ptr<VectorStore> in_store)
      : XXToolBase("rag_search", false, true), store(in_store) {}

  neograph::ChatTool get_definition() const override {
    return {
        "rag_search",
        R"(Search the knowledge base for relevant documents using semantic similarity. 
Use this tool to find information before answering questions. 
Returns the most relevant documents with their content, source, and similarity score.)",
        neograph::json{
            {"type", "object"},
            {
                "properties",
                {
                    {
                        "query",
                        {
                            {"type", "string"},
                            {"description",
                             "Search query to find relevant documents"},
                        },
                    },
                    {
                        "top_k",
                        {
                            {"type", "integer"},
                            {"description",
                             "Number of results to return (default: 3)"},
                        },
                    },
                },
            },
            {
                "required",
                neograph::json::array({"query"}),
            },
        },
    };
  }

  asio::awaitable<std::string>
  execute_async(const neograph::json &arguments) override {
    auto query = arguments.value("query", "");
    int top_k = arguments.value("top_k", 3);

    auto results = co_await store->search(query, top_k);

    if (false == results.has_value()) {
      co_return fmt::format("Search error: {}", results.error());
    }
    if (results->empty()) {
      co_return "No relevant documents found for: " + query;
    }

    auto output = neograph::json::array();
    for (const auto &[doc, contentIndex, score] : results.value()) {
      output.push_back({
          {"id", doc.id},
          {"title", doc.title},
          {"contentIndex", contentIndex},
          {"content", doc.content[contentIndex]},
          {"source", doc.source},
          {"similarity", std::round(score * 1000.0) / 1000.0},
      });
    }

    co_return output.dump(2);
  }

  inline static asio::awaitable<std::vector<Document>>
  scanDocument(const std::vector<std::string> &pathlist) {
    auto result = std::vector<Document>{};

    auto onAppendItem = [&](const std::string &path) -> bool {
      auto filepath = std::filesystem::path{path};
      if (filepath.extension() == ".md") {
        std::ifstream stream;
        try {
          stream.open(path);
          if (!stream) {
            auto ec = std::error_code{errno, std::system_category()};
            throw std::runtime_error{
                fmt::format(R"(Can not open file. Error: {})", ec.message())};
          }
          auto content = std::string{std::istreambuf_iterator<char>(stream),
                                     std::istreambuf_iterator<char>()};
          stream.close();

          result.push_back(Document{
              .id = std::to_string(result.size()),
              .title = filepath.filename(),
              .content = EmbeddingClient::splitStringByFixedLength(content),
              .source = path,
          });
          return true;
        } catch (const std::exception &e) {
          stream.close();
          XX_LOGD("RAG/scanDocument item exception: {} / {}", path, e.what());
        }
      }
      return false;
    };

    std::cout << "\n┏━━━━━━ RAG Docs Load ━━━━━━┓" << std::endl;
    for (const auto &itemPath : pathlist) {
      std::cout << "try: " << itemPath << std::endl;
      if (std::filesystem::is_directory(itemPath)) {
        for (const auto &entity :
             std::filesystem::recursive_directory_iterator(itemPath)) {
          if (entity.is_regular_file()) {
            if (onAppendItem(entity.path().c_str())) {
              auto &doc = result.back();
              fmt::println("┣━ ✅ Load success: `{}`(Block {} | {} )",
                           doc.title, doc.content.size(), itemPath);
              for (const auto &str : doc.content) {
                fmt::println("  - Size: {} | {}", str.size(),
                             agentxx::util::utf8GetLengthCheckAvail(str));
              }
            }
          }
        }
      } else if (std::filesystem::is_regular_file(itemPath)) {
        if (onAppendItem(itemPath)) {
          auto &doc = result.back();
          fmt::println("┣━ ✅ Load success: `{}`(Block {} | {} )", doc.title,
                       doc.content.size(), itemPath);
          for (const auto &str : doc.content) {
            fmt::println("  - Size: {} | {}", str.size(),
                         agentxx::util::utf8GetLengthCheckAvail(str));
          }
        }
      }
    }
    std::cout << "┗━━━━━━ RAG Docs Load ━━━━━━┛\n" << std::endl;

    co_return result;
  }
};

} // namespace tools
} // namespace agentxx