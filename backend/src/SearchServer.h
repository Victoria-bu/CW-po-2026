#pragma once

#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>

#include "DataLoader.h"
#include "InvertedIndex.h"
#include "OllamaClient.h"

// Статистика останньої побудови індексу (для GET /api/stats)
struct IndexStats {
  int documents = 0;
  long long indexBuildTimeMs = 0;
  int threadsUsed = 0;
};

// Результат побудови індексу (для POST /api/index)
struct IndexResult {
  bool success = false;
  int documentsIndexed = 0;
  long long timeMs = 0;
  bool alreadyRunning = false;
  std::string error;
};

class SearchServer {
 public:
  SearchServer(const std::string& dataPath, const std::string& ollamaHost,
               int ollamaPort, const std::string& ollamaModel);

  IndexResult BuildIndex(int numThreads);
  std::vector<std::string> ExpandQuery(const std::string& query);
  std::vector<SearchResult> SearchByTerms(const std::vector<std::string>& terms) const;
  std::vector<SearchResult> Search(const std::string& query) const;

  IndexStats GetStats() const;
  bool IsBuilding() const;
  bool IsOllamaAvailable();

 private:
  InvertedIndex invertedIndex;
  DataLoader dataLoader;
  OllamaClient ollamaClient;
  std::string dataPath;

  mutable std::shared_mutex indexAccessMutex;
  IndexStats stats;
  mutable std::mutex statsMutex;
  std::atomic<bool> buildInProgress{false};
};