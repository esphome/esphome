#pragma once

#include "http_request.h"

#ifdef USE_ESP32

#include <esp_event.h>
#include <esp_idf_version.h>
#include <esp_http_client.h>
#include <esp_netif.h>
#include <esp_tls.h>

// The socket is needed to wait for the response without blocking; esp_http_client_get_socket() is new in 5.5.3
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 3)
#define USE_HTTP_REQUEST_IDF_SOCKET_POLL
#endif

namespace esphome::http_request {

class HttpRequestIDF;

class HttpContainerIDF : public HttpContainer {
 public:
  HttpContainerIDF(esp_http_client_handle_t client) : client_(client) {}
  int read(uint8_t *buf, size_t max_len) override;
  int read_available(uint8_t *buf, size_t max_len) override;
  void end() override;
  bool is_read_complete() const override;
  HttpRequestProgress poll() override;

  /// @brief Feeds the watchdog timer if the executing task has one attached
  void feed_wdt();

 protected:
  friend class HttpRequestIDF;
  enum class Stage : uint8_t {
    STAGE_CONNECT,
    STAGE_SEND_BODY,
    STAGE_HEADERS,
    STAGE_DONE,
    STAGE_FAILED,
  };

  int read_(uint8_t *buf, size_t max_len);
  /// True when the socket can be read or written without waiting; always true when it cannot be checked
  bool socket_ready_(bool write);
  HttpRequestProgress wait_();
  HttpRequestProgress fail_(esp_err_t err);
  /// Finish with an unsuccessful status, which is reported through on_response like a success
  HttpRequestProgress response_failed_(int status_code);
  HttpRequestIDF *idf_parent_();

  esp_http_client_handle_t client_;
  // Owned copy (not a reference): must outlive perform() for the response-header event handler
  std::vector<std::string> collect_headers_;
  // Request body for begin(), released once sent
  std::string body_;
  size_t body_sent_{0};
  uint32_t timeout_ms_{0};
  // Time of the last step forward, for the request timeout of begin()
  uint32_t progress_time_{0};
  uint32_t start_time_{0};
  uint16_t redirects_left_{0};
  Stage stage_{Stage::STAGE_DONE};
  // An esp_http_client_open() of a non-blocking client returned ESP_ERR_HTTP_EAGAIN
  bool connecting_{false};
  bool async_{false};
};

class HttpRequestIDF final : public HttpRequestComponent {
  friend class HttpContainerIDF;

 public:
  // User provided, not "= default": `new(p) HttpRequestIDF()` would zero-fill .bss that is already zero.
  HttpRequestIDF() {}

  void dump_config() override;

  void set_buffer_size_rx(uint16_t buffer_size_rx) { this->buffer_size_rx_ = buffer_size_rx; }
  void set_buffer_size_tx(uint16_t buffer_size_tx) { this->buffer_size_tx_ = buffer_size_tx; }
  void set_verify_ssl(bool verify_ssl) { this->verify_ssl_ = verify_ssl; }
  void set_ca_certificate(const char *ca_certificate) { this->ca_certificate_ = ca_certificate; }

  std::shared_ptr<HttpContainer> begin(const char *url, const char *method, std::string &&body,
                                       const std::vector<Header> &request_headers,
                                       const std::vector<std::string> &lower_case_collect_headers,
                                       uint32_t timeout_ms) override;

 protected:
  /// A container that poll() takes from the connect through to the response headers
  std::shared_ptr<HttpContainerIDF> start_container_(const char *url, const char *method, std::string &&body,
                                                     const std::vector<Header> &request_headers,
                                                     const std::vector<std::string> &lower_case_collect_headers,
                                                     uint32_t timeout_ms, bool async);
  std::shared_ptr<HttpContainerIDF> create_container_(const char *url, const char *method,
                                                      const std::vector<Header> &request_headers,
                                                      const std::vector<std::string> &lower_case_collect_headers,
                                                      uint32_t timeout_ms, bool async);
  std::shared_ptr<HttpContainer> perform(const char *url, const char *method, const std::string &body,
                                         const std::vector<Header> &request_headers,
                                         const std::vector<std::string> &lower_case_collect_headers,
                                         uint32_t timeout_ms) override;
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
