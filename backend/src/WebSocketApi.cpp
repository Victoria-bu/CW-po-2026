#include "WebSocketApi.h"

#include <chrono>
#include <vector>

#include "ApiJson.h"

using json = nlohmann::json;

namespace {

bool SendStage(httplib::ws::WebSocket& ws, const std::string& stage, const json& data) {
  json message;
  message["stage"] = stage;
  message["data"] = data;
  return ws.send(apijson::Dump(message));
}

bool SendError(httplib::ws::WebSocket& ws, const std::string& text) {
  json data;
  data["message"] = text;
  return SendStage(ws, "error", data);
}

}  // namespace

WebSocketApi::WebSocketApi(SearchServer& searchServer) : searchServer(searchServer) {}

void WebSocketApi::Register(httplib::Server& http) {
  http.WebSocket("/ws/search", [this](const httplib::Request&, httplib::ws::WebSocket& ws) {
    HandleConnection(ws);
  });
}

void WebSocketApi::HandleConnection(httplib::ws::WebSocket& ws) {
  std::string message;
  while (ws.read(message)) {
    if (!HandleQuery(ws, message)) break;
  }
}

bool WebSocketApi::HandleQuery(httplib::ws::WebSocket& ws, const std::string& message) {
  std::string query;
  std::string mode = "keyword";
  try {
    json request = json::parse(message);
    query = request.at("query").get<std::string>();
    mode = request.value("mode", "keyword");
  } catch (...) {
    return SendError(ws, "expected JSON with a string query and mode");
  }
  if (mode != "keyword" && mode != "expanded") {
    return SendError(ws, "mode must be keyword or expanded");
  }
  if (query.empty()) {
    return SendError(ws, "query is empty");
  }
  if (searchServer.IsBuilding()) {
    return SendError(ws, "index is being built, try again later");
  }

  // Розширення запиту через Ollama
  std::vector<std::string> expandedTerms;

  if (mode == "expanded") {
    expandedTerms = searchServer.ExpandQuery(query);

    json expansion;
    expansion["original"] = query;
    expansion["expanded_terms"] = expandedTerms;

    if (!SendStage(ws, "query_expansion", expansion)) {
        return false;
    }
  }

  // Пошук в інвертованому індексі
  auto startTime = std::chrono::steady_clock::now();

  std::vector<std::string> terms = InvertedIndex::Tokenize(query);
  terms.insert(terms.end(), expandedTerms.begin(), expandedTerms.end());
  std::vector<SearchResult> results = searchServer.SearchByTerms(terms);

  auto elapsed = std::chrono::steady_clock::now() - startTime;

  json search;
  search["results"] = apijson::Results(results);
  search["time_ms"] = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

  if (!SendStage(ws, "keyword_search", search)) return false;

  // Завершення пошуку
  json done;
  done["stage"] = "done";
  return ws.send(apijson::Dump(done));
}