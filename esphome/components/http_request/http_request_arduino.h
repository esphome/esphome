#pragma once

#include "http_request.h"

#ifdef USE_ARDUINO

#if defined(USE_ESP32) || defined(USE_RP2040)
#include <HTTPClient.h>
#endif
#ifdef USE_ESP8266
#include <ESP8266HTTPClient.h>
#ifdef USE_HTTP_REQUEST_ESP8266_HTTPS
#include <WiFiClientSecure.h>
#endif
#endif

namespace esphome {
namespace http_request {

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
  std::shared_ptr<HttpContainer> perform(std::string url, std::string method, std::string body,
                                         std::list<Header> request_headers,
                                         std::set<std::string> collect_headers) override;
};

}  // namespace http_request
}  // namespace esphome

#endif  // USE_ARDUINO
