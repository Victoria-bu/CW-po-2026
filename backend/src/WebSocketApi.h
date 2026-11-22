#pragma once
#include <string>

#include "SearchServer.h"
#include "httplib.h"

class WebSocketApi {
 public:
  explicit WebSocketApi(SearchServer& searchServer);

  // Реєстрація WebSocket-ендпоінта /ws/search
  void Register(httplib::Server& http);

 private:
  // Обробка підключення та запитів клієнта
  void HandleConnection(httplib::ws::WebSocket& ws);
  bool HandleQuery(httplib::ws::WebSocket& ws, const std::string& message);

  SearchServer& searchServer;
};