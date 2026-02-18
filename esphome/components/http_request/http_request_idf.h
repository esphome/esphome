#pragma once

#include "http_request.h"

#ifdef USE_ESP32

#include <esp_event.h>
#include <esp_http_client.h>
#include <esp_netif.h>
#include <esp_tls.h>

namespace esphome::http_request {

class HttpContainerIDF : public HttpContainer {
 public:
  HttpContainerIDF(esp_http_client_handle_t client) : client_(client) {}
  int read(uint8_t *buf, size_t max_len) override;
  void end() override;
  bool is_read_complete() const override;

  /// @brief Feeds the watchdog timer if the executing task has one attached
  void feed_wdt();

 protected:
  friend class HttpRequestIDF;
  esp_http_client_handle_t client_;
};

class HttpRequestIDF : public HttpRequestComponent {
 public:
  void dump_config() override;

  void set_buffer_size_rx(uint16_t buffer_size_rx) { this->buffer_size_rx_ = buffer_size_rx; }
  void set_buffer_size_tx(uint16_t buffer_size_tx) { this->buffer_size_tx_ = buffer_size_tx; }
  void set_verify_ssl(bool verify_ssl) { this->verify_ssl_ = verify_ssl; }
  void set_ca_certificate(const char *ca_certificate) { this->ca_certificate_ = ca_certificate; }

 protected:
  std::shared_ptr<HttpContainer> perform(const std::string &url, const std::string &method, const std::string &body,
                                         const std::list<Header> &request_headers,
                                         const std::vector<std::string> &lower_case_collect_headers) override;
  // if zero ESP-IDF will use DEFAULT_HTTP_BUF_SIZE
  uint16_t buffer_size_rx_{};
  uint16_t buffer_size_tx_{};
  bool verify_ssl_{true};
  const char *ca_certificate_{nullptr};

  /// @brief Monitors the http client events to gather response headers
  static esp_err_t http_event_handler(esp_http_client_event_t *evt);
};

}  // namespace esphome::http_request

#endif  // USE_ESP32
