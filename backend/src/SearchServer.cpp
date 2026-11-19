#include "SearchServer.h"

#include <chrono>
#include <exception>
#include <filesystem>
#include <set>

#include "ThreadPool.h"

SearchServer::SearchServer(const std::string& dataPath,
                           const std::string& ollamaHost, int ollamaPort,
                           const std::string& ollamaModel)
    : dataLoader(dataPath), ollamaClient(ollamaHost, ollamaPort, ollamaModel), dataPath(dataPath) {}

IndexResult SearchServer::BuildIndex(int numThreads) {
  IndexResult result;

  if (buildInProgress.exchange(true)) {
    result.alreadyRunning = true;
    result.error = "index build is already running";
    return result;
  }

  try {
    if (numThreads < 1) {
      result.error = "threads must be greater than zero";
    } else {
      std::unique_lock<std::shared_mutex> lock(indexAccessMutex);
      auto startTime = std::chrono::steady_clock::now();

      std::vector<std::string> files = dataLoader.ListFiles();
      if (files.empty()) {
        result.error = "no .txt files found in the data directory";
      } else {
        invertedIndex.Clear();

        {
          ThreadPool pool(numThreads);
          for (int i = 0; i < static_cast<int>(files.size()); ++i) {
            pool.AddTask([this, &files, i] {
              Document doc;
              doc.id = i;
              doc.filename = files[i];
              doc.text = dataLoader.ReadFile(files[i]);

              invertedIndex.AddDocument(doc);
            });
          }
          pool.WaitUntilDone();
        }

        auto elapsed = std::chrono::steady_clock::now() - startTime;
        long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        int indexed = invertedIndex.DocumentCount();  // скільки документів справді додано

        {
          std::lock_guard<std::mutex> statsLock(statsMutex);
          stats = {indexed, ms, numThreads};
        }

        result.success = true;
        result.documentsIndexed = indexed;
        result.timeMs = ms;
      }
    }
  } catch (const std::exception& error) {
    result.error = error.what();
  } catch (...) {
    result.error = "unexpected error while building the index";
  }

  buildInProgress = false;
  return result;
}

std::vector<std::string> SearchServer::ExpandQuery(const std::string& query) {
  std::set<std::string> seen;
  for (const std::string& word : InvertedIndex::Tokenize(query)) {
    seen.insert(word);
  }

  std::vector<std::string> expanded;
  for (const std::string& phrase : ollamaClient.ExpandQuery(query)) {
    for (const std::string& word : InvertedIndex::Tokenize(phrase)) {
      if (seen.insert(word).second) {
        expanded.push_back(word);
      }
    }
  }
  return expanded;
}

std::vector<SearchResult> SearchServer::SearchByTerms(
    const std::vector<std::string>& terms) const {
  std::shared_lock<std::shared_mutex> lock(indexAccessMutex);
  return invertedIndex.Search(terms);
}

std::vector<SearchResult> SearchServer::Search(const std::string& query) const {
  return SearchByTerms(InvertedIndex::Tokenize(query));
}

IndexStats SearchServer::GetStats() const {
  std::lock_guard<std::mutex> lock(statsMutex);
  return stats;
}

bool SearchServer::IsBuilding() const {
  return buildInProgress.load();
}

bool SearchServer::IsOllamaAvailable() {
  return ollamaClient.IsAvailable();
}