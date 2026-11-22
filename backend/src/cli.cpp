#include <iostream>
#include <string>

#include "httplib.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cout << "Usage: ./search \"query\" [keyword|expanded]\n";
    return 1;
  }

  std::string query = argv[1];
  std::string mode = argc >= 3 ? argv[2] : "keyword";

  if (mode != "keyword" && mode != "expanded") {
    std::cerr << "Invalid search mode\n";
    return 1;
  }

  httplib::Client client("localhost", 8080);
  client.set_read_timeout(120);

  json request = {
      {"query", query},
      {"mode", mode}
  };

  auto response = client.Post(
      "/api/search",
      request.dump(),
      "application/json"
  );

  if (!response) {
    std::cerr << "Cannot connect to search server\n";
    return 1;
  }

  if (response->status != 200) {
    std::cerr << "Server error: " << response->status
              << "\n" << response->body << "\n";
    return 1;
  }

  json data;

  try {
    data = json::parse(response->body);
  } catch (const std::exception&) {
    std::cerr << "Invalid server response\n";
    return 1;
  }

  std::cout << "Query: " << query << "\n";
  std::cout << "Mode: " << mode << "\n";
  std::cout << "Time: " << data.value("time_ms", 0LL) << " ms\n";

  if (data.contains("expanded_terms")) {
    std::cout << "Expanded terms: ";
    for (const auto& term : data["expanded_terms"]) {
      std::cout << term.get<std::string>() << " ";
    }
    std::cout << "\n";
  }

  const auto& results = data.at("results");
  std::cout << "Found: " << results.size() << " documents\n\n";

  for (const auto& result : results) {
    std::cout << "Document: "
              << result.at("doc_id").get<std::string>() << "\n";

    std::cout << "Score: "
              << result.at("score").get<double>() << "\n";

    std::string snippet =
        result.value("snippet", std::string(""));

    if (snippet.size() > 200) {
      snippet = snippet.substr(0, 200) + "...";
    }

    std::cout << snippet << "\n\n";
  }

  return 0;
}