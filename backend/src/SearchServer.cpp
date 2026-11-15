#include "SearchServer.h"

#include <chrono>
#include <vector>

#include "ThreadPool.h"

SearchServer::SearchServer(const std::string& dataPath) : dataLoader(dataPath) {}

IndexResult SearchServer::BuildIndex(int numThreads) {

  if (buildInProgress.exchange(true)) {
    IndexResult busy;
    busy.alreadyRunning = true;
    return busy;
  }

  auto startTime = std::chrono::steady_clock::now();

  invertedIndex.Clear();
  std::vector<std::string> files = dataLoader.ListFiles();
  int total = static_cast<int>(files.size());
  if (total == 0) {
    buildInProgress = false;
    return {false, 0, 0, false};
  }

  {
    ThreadPool pool(numThreads);
    for (int i = 0; i < total; i++) {
      pool.AddTask([this, &files, i] {
        invertedIndex.AddDocument(i, DataLoader::ReadFile(files[i]));
      });
    }
    pool.WaitUntilDone();
  }

  auto elapsed = std::chrono::steady_clock::now() - startTime;
  long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

  int indexed = invertedIndex.DocumentCount();  // скільки документів справді додано

  {
    std::lock_guard<std::mutex> lock(statsMutex);
    stats = {indexed, ms, numThreads};
  }

  buildInProgress = false;
  return {true, indexed, ms, false};
}

IndexStats SearchServer::GetStats() const {
  std::lock_guard<std::mutex> lock(statsMutex);
  return stats;
}