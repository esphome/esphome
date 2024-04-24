#pragma once

#include "http_request.h"

#ifdef USE_ESP_IDF

#include <esp_event.h>
#include <esp_http_client.h>
#include <esp_netif.h>
#include <esp_tls.h>

namespace esphome {
namespace http_request {

static const size_t RESPONSE_BUFFER_SIZE = 2048;

class HttpRequestIDF : public HttpRequestComponent {
 public:
  void set_url(std::string url) override;
  HttpResponse send() override;

  void set_tx_buffer_size(int tx_buffer_size) { this->tx_buffer_size_ = tx_buffer_size; }
  void set_rx_buffer_size(int rx_buffer_size) { this->rx_buffer_size_ = rx_buffer_size; }

 protected:
  char last_response_buffer_[RESPONSE_BUFFER_SIZE];
  int last_status_code_ = 0;
  int last_content_length_ = 0;

  int tx_buffer_size_;
  int rx_buffer_size_;
};

}  // namespace http_request
}  // namespace esphome

#endif  // USE_ESP_IDF
