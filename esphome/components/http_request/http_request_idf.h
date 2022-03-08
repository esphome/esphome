#pragma once

#include "http_request.h"

#ifdef USE_ESP_IDF

#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_netif.h"
#include "esp_tls.h"

namespace esphome {
namespace http_request {

static const size_t RESPONSE_BUFFER_SIZE = 2048;

class HttpRequestIDF : public HttpRequestComponent {
 public:
  void set_url(std::string url) override;
  std::unique_ptr<HttpResponse> send() override;

 protected:
  char last_response_buffer_[RESPONSE_BUFFER_SIZE];
  int last_status_code_ = 0;
  int last_content_length_ = 0;
};

}  // namespace http_request
}  // namespace esphome

#endif  // USE_ESP_IDF
