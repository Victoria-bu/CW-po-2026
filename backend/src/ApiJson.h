#pragma once
#include <string>
#include <vector>

#include "InvertedIndex.h"
#include "nlohmann/json.hpp"

// Спільні функції для REST і WebSocket, щоб обидва повертали результати однаково
namespace apijson {

// [{"doc_id": "...", "score": 2, "snippet": "..."}, ...]
inline nlohmann::json Results(const std::vector<SearchResult>& results) {
  nlohmann::json items = nlohmann::json::array();
  for (const SearchResult& result : results) {
    nlohmann::json item;
    item["doc_id"] = result.docId;
    item["score"] = result.score;
    item["snippet"] = result.snippet;
    items.push_back(item);
  }
  return items;
}

// JSON у рядок. Байти, які не є коректним UTF-8, замінюються, а не кидають виняток
inline std::string Dump(const nlohmann::json& value) {
  return value.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
}

}