#pragma once

#include "http_request.h"

#if defined(USE_ARDUINO) && !defined(USE_ESP32)

#if defined(USE_RP2040)
#include <HTTPClient.h>
#include <WiFiClient.h>
#endif
#ifdef USE_ESP8266
#include <ESP8266HTTPClient.h>
#ifdef USE_HTTP_REQUEST_ESP8266_HTTPS
#include <WiFiClientSecure.h>
#endif
#endif

namespace esphome::http_request {

class HttpRequestArduino;
class HttpContainerArduino : public HttpContainer {
 public:
  int read(uint8_t *buf, size_t max_len) override;
  void end() override;

 protected:
  friend class HttpRequestArduino;
  HTTPClient client_{};
};

class HttpRequestArduino : public HttpRequestComponent {
 protected:
  std::shared_ptr<HttpContainer> perform(const std::string &url, const std::string &method, const std::string &body,
                                         const std::list<Header> &request_headers,
                                         const std::set<std::string> &collect_headers) override;
};

}  // namespace esphome::http_request

#endif  // USE_ARDUINO && !USE_ESP32
