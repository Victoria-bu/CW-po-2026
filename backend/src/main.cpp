#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>

#include "SearchServer.h"
#include "OllamaClient.h"
#include "httplib.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

// Читає змінну середовища з docker-compose.yml (якщо її нема, бере значення за замовчуванням)
static std::string Env(const char* name, const char* fallback) {
  const char* value = std::getenv(name);
  return value ? value : fallback;
}

int main() {
  std::string dataPath = Env("DATA_PATH", "/data");
  std::string ollamaHost = Env("OLLAMA_HOST", "ollama");
  int ollamaPort = std::stoi(Env("OLLAMA_PORT", "11434"));
  int httpPort = std::stoi(Env("HTTP_PORT", "8080"));

  OllamaClient ollama(ollamaHost, ollamaPort);
  SearchServer searchServer(dataPath);
  httplib::Server server;

  // GET /api/health
  server.Get("/api/health", [&](const httplib::Request&, httplib::Response& response) {
    json answer = {{"status", "ok"}, {"ollama", ollama.IsAvailable()}};
    response.set_content(answer.dump(), "application/json");
  });

  // POST /api/index  { "threads": K }
  server.Post("/api/index", [&](const httplib::Request& request, httplib::Response& response) {
    int threads = 4;
    try {
      threads = json::parse(request.body).value("threads", 4);
    } catch (...) {}  // порожнє або некоректне тіло: лишаємо 4 потоки
    threads = std::clamp(threads, 1, 64);

    IndexResult result = searchServer.BuildIndex(threads);
    json answer = {{"status", result.success ? "ok" : "error"},
                   {"documents_indexed", result.documentsIndexed},
                   {"time_ms", result.timeMs}};
    response.set_content(answer.dump(), "application/json");
  });

  // GET /api/stats
  server.Get("/api/stats", [&](const httplib::Request&, httplib::Response& response) {
    IndexStats stats = searchServer.GetStats();
    json answer = {{"documents", stats.documents},
                   {"index_build_time_ms", stats.indexBuildTimeMs},
                   {"threads_used", stats.threadsUsed}};
    response.set_content(answer.dump(), "application/json");
  });

  std::cout << "Server listening on port " << httpPort << std::endl;
  server.listen("0.0.0.0", httpPort);
}