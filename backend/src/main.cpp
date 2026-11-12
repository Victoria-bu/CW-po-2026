#include <cstdlib>
#include <iostream>
#include <string>

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
  std::string ollamaHost = Env("OLLAMA_HOST", "ollama");
  int ollamaPort = std::stoi(Env("OLLAMA_PORT", "11434"));
  int httpPort = std::stoi(Env("HTTP_PORT", "8081"));

  OllamaClient ollama(ollamaHost, ollamaPort);
  httplib::Server server;

  // Чи працює сервер і чи доступна Ollama
  server.Get("/api/health", [&](const httplib::Request&, httplib::Response& response) {
    json answer = {{"status", "ok"}, {"ollama", ollama.IsAvailable()}};
    response.set_content(answer.dump(), "application/json");
  });

  std::cout << "Server listening on port " << httpPort << std::endl;
  server.listen("0.0.0.0", httpPort);
}