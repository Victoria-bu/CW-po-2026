#pragma once
#include "SearchServer.h"
#include "httplib.h"

class WebApi {
 public:
  explicit WebApi(SearchServer& searchServer);
  void Register(httplib::Server& http);

 private:
  void Health(const httplib::Request& request, httplib::Response& response);
  void Index(const httplib::Request& request, httplib::Response& response);
  void Search(const httplib::Request& request, httplib::Response& response);
  void Stats(const httplib::Request& request, httplib::Response& response);

  SearchServer& searchServer;
};