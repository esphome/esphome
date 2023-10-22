#pragma once

#include "ota_http.h"

#ifdef USE_ARDUINO

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"

#include <memory>
#include <utility>
#include <string>

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
namespace ota_http {

class OtaHttpArduino : public OtaHttpComponent {
 public:
  int http_init() override;
  size_t http_read(uint8_t *buf, size_t len) override;
  void http_end() override;

 protected:
  HTTPClient client_{};
  WiFiClient stream;  // needed for 8266
  WiFiClient *streamPtr = &stream;
};

}  // namespace ota_http
}  // namespace esphome

#endif  // USE_ARDUINO
