#pragma once

#include <array>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "DataLoader.h"

struct SearchResult {
  std::string docId;
  double score = 0; 
  std::string snippet; 
};

struct Posting {
  int docId = 0;
  int count = 0;
};

class IndexShard {
 public:
  void AddTerm(const std::string& term, int docId, int count);
  std::vector<Posting> GetPostings(const std::string& term) const;
  void Clear();

 private:
  std::unordered_map<std::string, std::vector<Posting>> terms;
  mutable std::shared_mutex mutex;
};

class InvertedIndex {
 public:
  static std::vector<std::string> Tokenize(const std::string& text);

  void AddDocument(const Document& doc);
  std::vector<SearchResult> Search(const std::vector<std::string>& words) const;
  void Clear();
  int DocumentCount() const;

 private:
  static constexpr std::size_t kShardCount = 64;

  static std::size_t ShardIndex(const std::string& term);

  struct DocInfo {
    std::string filename;
    std::string snippet;
  };

  std::array<IndexShard, kShardCount> shards;
  std::unordered_map<int, DocInfo> docs;

  mutable std::shared_mutex docsMutex;
  mutable std::shared_mutex indexMutex;
};