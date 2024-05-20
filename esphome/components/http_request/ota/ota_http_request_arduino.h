#pragma once

#include "ota_http_request.h"

#ifdef USE_ARDUINO
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"

#include <memory>
#include <string>
#include <utility>

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

class OtaHttpRequestComponentArduino : public OtaHttpRequestComponent {
 public:
  void http_init(const std::string &url) override;
  int http_read(uint8_t *buf, size_t len) override;
  void http_end() override;

 protected:
  int set_stream_ptr_();
  HTTPClient client_{};
  std::unique_ptr<WiFiClient> stream_ptr_;
};

}  // namespace http_request
}  // namespace esphome

#endif  // USE_ARDUINO
