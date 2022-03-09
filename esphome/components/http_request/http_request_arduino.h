#pragma once

#include "http_request.h"

#ifdef USE_ARDUINO

#ifdef USE_ESP32
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

class HttpRequestArduino : public HttpRequestComponent {
 public:
  void set_url(std::string url) override;
  HttpResponse send() override;

 protected:
  std::string last_url_;
  HTTPClient client_{};
#ifdef USE_ESP8266
  std::shared_ptr<WiFiClient> wifi_client_;
#ifdef USE_HTTP_REQUEST_ESP8266_HTTPS
  std::shared_ptr<BearSSL::WiFiClientSecure> wifi_client_secure_;
#endif
  std::shared_ptr<WiFiClient> get_wifi_client_();
#endif
};

}  // namespace http_request
}  // namespace esphome

#endif  // USE_ARDUINO
