#pragma once
// A handler registered after setup(), the way an external component would; answers GET /probe.
#include "esphome/components/web_server_base/web_server_base.h"
class ProbeHandler : public AsyncWebHandler {
 public:
  bool canHandle(AsyncWebServerRequest *request) const override {
    if (request->method() != HTTP_GET)
      return false;
#ifdef USE_ESP32
    char url_buf[AsyncWebServerRequest::URL_BUF_SIZE];
    return request->url_to(url_buf) == "/probe";
#else
    return request->url() == ESPHOME_F("/probe");
#endif
  }
  void handleRequest(AsyncWebServerRequest *request) override { request->send(200, "text/plain", "probe-ok"); }
};
inline ProbeHandler probe_handler;
