#include "OllamaClient.h"

#include "httplib.h"

OllamaClient::OllamaClient(const std::string& host, int port)
    : host_(host), port_(port) {}

bool OllamaClient::IsAvailable() {
  httplib::Client client(host_, port_);
  client.set_connection_timeout(2); 
  auto response = client.Get("/api/tags");
  return response && response->status == 200;
}