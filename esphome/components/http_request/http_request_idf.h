#pragma once

#include "http_request.h"

#ifdef USE_ESP_IDF

#include <esp_event.h>
#include <esp_http_client.h>
#include <esp_netif.h>
#include <esp_tls.h>

namespace esphome {
namespace http_request {

class HttpContainerIDF : public HttpContainer {
 public:
  HttpContainerIDF(esp_http_client_handle_t client) : client_(client) {}
  int read(uint8_t *buf, size_t max_len) override;
  void end() override;

  /// @brief Feeds the watchdog timer if the executing task has one attached
  void feed_wdt();

  void set_response_headers(std::map<std::string, std::list<std::string>> &response_headers) {
    this->response_headers_ = std::move(response_headers);
  }

 protected:
  esp_http_client_handle_t client_;
};

class HttpRequestIDF : public HttpRequestComponent {
 public:
  void dump_config() override;

  void set_buffer_size_rx(uint16_t buffer_size_rx) { this->buffer_size_rx_ = buffer_size_rx; }
  void set_buffer_size_tx(uint16_t buffer_size_tx) { this->buffer_size_tx_ = buffer_size_tx; }

 protected:
  std::shared_ptr<HttpContainer> perform(std::string url, std::string method, std::string body,
                                         std::list<Header> request_headers,
                                         std::set<std::string> collect_headers) override;
  // if zero ESP-IDF will use DEFAULT_HTTP_BUF_SIZE
  uint16_t buffer_size_rx_{};
  uint16_t buffer_size_tx_{};

  /// @brief Monitors the http client events to gather response headers
  static esp_err_t http_event_handler(esp_http_client_event_t *evt);
};

}  // namespace http_request
}  // namespace esphome

#endif  // USE_ESP_IDF
