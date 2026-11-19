
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "SearchServer.h"
#include "httplib.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

static std::string Env(const char* name, const char* fallback) {
  const char* value = std::getenv(name);
  return value ? value : fallback;
}

static void SendJson(httplib::Response& response, int status, const json& body) {
  response.status = status;
  response.set_content(
      body.dump(-1, ' ', false, json::error_handler_t::replace),
      "application/json");
}

static void SendError(httplib::Response& response, int status,
                      const std::string& message) {
  json answer = {{"status", "error"}, {"message", message}};
  SendJson(response, status, answer);
}

int main() {
  std::string dataPath = Env("DATA_PATH", "/data");
  std::string ollamaHost = Env("OLLAMA_HOST", "ollama");
  int ollamaPort = std::stoi(Env("OLLAMA_PORT", "11434"));
  std::string ollamaModel = Env("OLLAMA_MODEL", "llama3.2:3b");
  int httpPort = std::stoi(Env("HTTP_PORT", "8080"));

  SearchServer searchServer(dataPath, ollamaHost, ollamaPort, ollamaModel);
  httplib::Server server;

  // GET /api/health
  server.Get("/api/health", [&](const httplib::Request&,
                                httplib::Response& response) {
    json answer = {
        {"status", "ok"},
        {"ollama", searchServer.IsOllamaAvailable()}
    };
    SendJson(response, 200, answer);
  });

  // POST /api/index
  server.Post("/api/index", [&](const httplib::Request& request,
                                httplib::Response& response) {
    int threads = 4;

    try {
      json body = json::parse(request.body);
      threads = body.value("threads", 4);
    } catch (const std::exception&) {
      SendError(response, 400, "invalid JSON or threads parameter");
      return;
    }

    threads = std::clamp(threads, 1, 64);

    IndexResult result = searchServer.BuildIndex(threads);

    if (result.alreadyRunning) {
      SendError(response, 409, result.error);
      return;
    }

    if (!result.success) {
      SendError(response, 500, result.error);
      return;
    }

    json answer = {
        {"status", "ok"},
        {"documents_indexed", result.documentsIndexed},
        {"time_ms", result.timeMs}
    };
    SendJson(response, 200, answer);
  });

  // POST /api/search
  server.Post("/api/search", [&](const httplib::Request& request, httplib::Response& response) {
    std::string query;
    std::string mode;

    try {
      json body = json::parse(request.body);
      query = body.value("query", "");
      mode = body.value("mode", "keyword");
    } catch (const std::exception&) {
      SendError(response, 400, "invalid search request");
      return;
    }

    if (query.empty()) {
      SendError(response, 400, "query is empty");
      return;
    }

    if (mode != "keyword" && mode != "expanded") {
      SendError(response, 400, "invalid search mode");
      return;
    }

    if (searchServer.IsBuilding()) {
      SendError(response, 503, "index is being built");
      return;
    }

    auto startTime = std::chrono::steady_clock::now();

    std::vector<SearchResult> results;
    std::vector<std::string> expandedTerms;

    if (mode == "expanded") {
      std::vector<std::string> terms = InvertedIndex::Tokenize(query);
      expandedTerms = searchServer.ExpandQuery(query);

      if (searchServer.IsBuilding()) {
        SendError(response, 503, "index is being built");
        return;
      }

      terms.insert(terms.end(), expandedTerms.begin(), expandedTerms.end());
      results = searchServer.SearchByTerms(terms);
    } else {
      results = searchServer.Search(query);
    }

    auto elapsed = std::chrono::steady_clock::now() - startTime;
    long long ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    json items = json::array();

    for (const SearchResult& result : results) {
      items.push_back({
          {"doc_id", result.docId},
          {"score", result.score},
          {"snippet", result.snippet}
      });
    }

    json answer = {
        {"results", items},
        {"time_ms", ms}
    };

    if (mode == "expanded") {
      answer["expanded_terms"] = expandedTerms;
    }

    SendJson(response, 200, answer);
  });

  // GET /api/stats
  server.Get("/api/stats", [&](const httplib::Request&, httplib::Response& response) {
    IndexStats stats = searchServer.GetStats();

    json answer = {
        {"documents", stats.documents},
        {"index_build_time_ms", stats.indexBuildTimeMs},
        {"threads_used", stats.threadsUsed}
    };

    SendJson(response, 200, answer);
  });

  std::cout << "Server listening on port " << httpPort << std::endl;

  if (!server.listen("0.0.0.0", httpPort)) {
    std::cerr << "Failed to start server" << std::endl;
    return 1;
  }

  return 0;
}
