#pragma once
#include <atomic>
#include <map>
#include <shared_mutex>
#include <string>
#include <vector>
#include <unordered_map>
#include <utility>

// Один знайдений документ
struct SearchResult {
  int docId = 0;
  double score = 0;  // скільки різних слів запиту знайдено в документі
  std::string snippet;  // початок тексту документа
};

class InvertedIndex {
 public:
  static std::vector<std::string> Tokenize(const std::string& text);
  void AddDocument(int id, const std::string& text);
  std::vector<SearchResult> Search(const std::vector<std::string>& words) const;
  void Clear();
  int DocumentCount() const;

 private:
  static const int kMaxResults = 20;  // скільки результатів повертаємо

  std::unordered_map<std::string, std::vector<std::pair<int, int>>> index;
  std::unordered_map<int, std::string> snippets;

  mutable std::shared_mutex indexLock;
  std::atomic<int> documentCount{0};
};