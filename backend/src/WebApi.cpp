#include "WebApi.h"

#include <algorithm>
#include <chrono>
#include <string>
#include <vector>

#include "ApiJson.h"

using json = nlohmann::json;

namespace {

void SendJson(httplib::Response& response, int status, const json& body) {
  response.status = status;
  response.set_content(apijson::Dump(body), "application/json");
}

void SendError(httplib::Response& response, int status, const std::string& message) {
  json answer;
  answer["status"] = "error";
  answer["message"] = message;
  SendJson(response, status, answer);
}

long long ElapsedMs(std::chrono::steady_clock::time_point start) {
  auto elapsed = std::chrono::steady_clock::now() - start;
  return std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
}

}  // namespace

WebApi::WebApi(SearchServer& searchServer) : searchServer(searchServer) {}

void WebApi::Register(httplib::Server& http) {
  http.Get("/api/health", [this](const httplib::Request& request, httplib::Response& response) {
    Health(request, response);
  });
  http.Post("/api/index", [this](const httplib::Request& request, httplib::Response& response) {
    Index(request, response);
  });
  http.Post("/api/search", [this](const httplib::Request& request, httplib::Response& response) {
    Search(request, response);
  });
  http.Get("/api/stats", [this](const httplib::Request& request, httplib::Response& response) {
    Stats(request, response);
  });
}

void WebApi::Health(const httplib::Request&, httplib::Response& response) {
  json answer;
  answer["status"] = "ok";
  answer["ollama"] = searchServer.IsOllamaAvailable();
  SendJson(response, 200, answer);
}

void WebApi::Index(const httplib::Request& request, httplib::Response& response) {
  int threads = 4;
  try {
    threads = json::parse(request.body).value("threads", 4);
  } catch (...) {
    SendError(response, 400, "invalid JSON body or threads value");
    return;
  }

  // Обмежуємо кількість потоків від 1 до 64
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

  json answer;
  answer["status"] = "ok";
  answer["documents_indexed"] = result.documentsIndexed;
  answer["time_ms"] = result.timeMs;

  SendJson(response, 200, answer);
}

void WebApi::Search(const httplib::Request& request, httplib::Response& response) {
  std::string query;
  std::string mode;
  
  try {
    json body = json::parse(request.body);
    query = body.value("query", "");
    mode = body.value("mode", "keyword");
  } catch (...) {
    SendError(response, 400, "invalid JSON body");
    return;
  }

  if (query.empty()) {
    SendError(response, 400, "query is empty");
    return;
  }

  if (mode != "keyword" && mode != "expanded") {
    SendError(response, 400, "mode must be keyword or expanded");
    return;
  }

  if (searchServer.IsBuilding()) {
    SendError(response, 503, "index is being built, try again later");
    return;
  }

  auto startTime = std::chrono::steady_clock::now();

  std::vector<SearchResult> results;
  std::vector<std::string> expandedTerms;

  if (mode == "expanded") {
    expandedTerms = searchServer.ExpandQuery(query);

    std::vector<std::string> terms = InvertedIndex::Tokenize(query);
    terms.insert(terms.end(), expandedTerms.begin(), expandedTerms.end());

    results = searchServer.SearchByTerms(terms);
  } else {
    results = searchServer.Search(query);
  }

  json answer;
  answer["results"] = apijson::Results(results);
  answer["time_ms"] = ElapsedMs(startTime);

  if (mode == "expanded") {
    answer["expanded_terms"] = expandedTerms; 
  }
  SendJson(response, 200, answer);
}

void WebApi::Stats(const httplib::Request&, httplib::Response& response) {
  IndexStats stats = searchServer.GetStats();
  json answer;

  answer["documents"] = stats.documents;
  answer["index_build_time_ms"] = stats.indexBuildTimeMs;
  answer["threads_used"] = stats.threadsUsed;

  SendJson(response, 200, answer);
}