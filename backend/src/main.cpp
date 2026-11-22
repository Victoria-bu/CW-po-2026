#include <cstdlib>
#include <iostream>
#include <string>

#include "SearchServer.h"
#include "WebApi.h"
#include "WebSocketApi.h"
#include "httplib.h"

static std::string Env(const char* name, const char* fallback) {
  const char* value = std::getenv(name);
  return value ? value : fallback;
}

int main() {
  std::string dataPath = Env("DATA_PATH", "/data");
  std::string webPath = Env("WEB_PATH", "./web");
  std::string ollamaHost = Env("OLLAMA_HOST", "ollama");
  int ollamaPort = std::stoi(Env("OLLAMA_PORT", "11434"));
  std::string ollamaModel = Env("OLLAMA_MODEL", "llama3.2:3b");
  int httpPort = std::stoi(Env("HTTP_PORT", "8080"));

  SearchServer searchServer(dataPath, ollamaHost, ollamaPort, ollamaModel);
  WebApi webApi(searchServer);
  WebSocketApi webSocketApi(searchServer);

  httplib::Server server;
  webApi.Register(server);       
  webSocketApi.Register(server);

  // Веб-інтерфейс  http://localhost:8080/
  if (!server.set_mount_point("/", webPath)) {
    std::cout << "Web UI folder not found: " << webPath << std::endl;
  }

  std::cout << "Server listening on port " << httpPort << std::endl;
  server.listen("0.0.0.0", httpPort);
}
