#pragma once

#include "ota_http_request.h"

#ifdef USE_ESP_IDF
#include "esp_http_client.h"

namespace esphome {
namespace ota_http_request {

class OtaHttpRequestComponentIDF : public OtaHttpRequestComponent {
 public:
  void http_init() override;
  int http_read(uint8_t *buf, size_t len) override;
  void http_end() override;

 protected:
  esp_http_client_handle_t client_{};
};

}  // namespace ota_http_request
}  // namespace esphome

#endif  // USE_ESP_IDF
