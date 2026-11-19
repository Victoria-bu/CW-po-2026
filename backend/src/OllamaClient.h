#pragma once

#include <mutex>
#include <string>
#include <vector>

class OllamaClient {
 public:
  OllamaClient(const std::string& host, int port, const std::string& model);

  bool IsAvailable();
  std::vector<std::string> ExpandQuery(const std::string& query);

 private:
  std::string host;
  int port;
  std::string model;
  std::mutex requestMutex;
};