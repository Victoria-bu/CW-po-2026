#pragma once
#include <string>

// Клієнт для звернень до Ollama
class OllamaClient {
 public:
  OllamaClient(const std::string& host, int port);
  bool IsAvailable();

 private:
  std::string host_;
  int port_;
};