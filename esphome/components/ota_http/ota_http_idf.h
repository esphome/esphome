#pragma once

#include "ota_http.h"

#ifdef USE_ESP_IDF

#include "esp_http_client.h"

namespace esphome {
namespace ota_http {

class OtaHttpIDF : public OtaHttpComponent {
 public:
  int http_init() override;
  size_t http_read(uint8_t *buf, size_t len) override;
  void http_end() override;

 protected:
  esp_http_client_handle_t client_{};
};

}  // namespace ota_http
}  // namespace esphome

#endif  // USE_ESP_IDF
