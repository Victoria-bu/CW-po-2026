
#include "OllamaClient.h"

#include <exception>
#include <iostream>

#include "httplib.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

static const int kMaxExpandedTerms = 8;

OllamaClient::OllamaClient(const std::string& host, int port, const std::string& model)
    : host(host), port(port), model(model) {}

bool OllamaClient::IsAvailable() {
  httplib::Client client(host, port);
  client.set_connection_timeout(2);
  client.set_read_timeout(5);

  auto response = client.Get("/api/tags");
  return response && response->status == 200;
}

std::vector<std::string> OllamaClient::ExpandQuery(const std::string& query) {
  if (query.empty()) {
    return {};
  }

  std::string prompt =
      "You help a search engine over English text documents. "
      "Give up to 8 English keywords and synonyms related to the query. "
      "The query may be in any language. "
      "Answer only with JSON like {\"terms\": [\"word1\", \"word2\"]}.\n"
      "Query: " + query;

  json request;
  request["model"] = model;
  request["prompt"] = prompt;
  request["stream"] = false;
  request["format"] = "json";
  request["options"]["temperature"] = 0;

  std::string responseBody;

  try {
    std::lock_guard<std::mutex> lock(requestMutex);

    httplib::Client client(host, port);
    client.set_connection_timeout(5);
    client.set_read_timeout(120);

    std::string body =
        request.dump(-1, ' ', false, json::error_handler_t::replace);

    auto response = client.Post("/api/generate", body, "application/json");

    if (!response) {
      std::cerr << "Ollama: no response" << std::endl;
      return {};
    }

    if (response->status != 200) {
      std::cerr << "Ollama: HTTP error " << response->status << std::endl;
      return {};
    }

    responseBody = response->body;

    json outer = json::parse(responseBody);
    json inner = json::parse(outer.at("response").get<std::string>());

    if (!inner.contains("terms") || !inner["terms"].is_array()) {
      return {};
    }

    std::vector<std::string> terms;

    for (const auto& term : inner["terms"]) {
      if (static_cast<int>(terms.size()) >= kMaxExpandedTerms) {
        break;
      }

      if (term.is_string()) {
        terms.push_back(term.get<std::string>());
      }
    }

    return terms;

  } catch (const std::exception& error) {
    std::cerr << "Ollama: " << error.what() << std::endl;
    return {};
  }
}
