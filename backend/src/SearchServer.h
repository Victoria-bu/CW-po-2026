#pragma once
#include <atomic>
#include <mutex>
#include <string>

#include "DataLoader.h"
#include "InvertedIndex.h"

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
};

class SearchServer {
 public:
  explicit SearchServer(const std::string& dataPath);

  IndexResult BuildIndex(int numThreads);
  IndexStats GetStats() const;

 private:
  InvertedIndex invertedIndex;
  DataLoader dataLoader;
  IndexStats stats;
  mutable std::mutex statsMutex;
  std::atomic<bool> buildInProgress{false}; 
};