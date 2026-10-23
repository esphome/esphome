#include "http_request_idf.h"

#ifdef USE_ESP32

#include <algorithm>
#include <cerrno>
#include <cstring>

#include <fcntl.h>
#include <sys/select.h>

#include "esphome/components/network/util.h"
#include "esphome/components/watchdog/watchdog.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#include "esp_crt_bundle.h"
#endif

#include "esp_task_wdt.h"

namespace esphome::http_request {

ESPHOME_LOG_TAG(TAG, "http_request");

// Request body sent per esp_http_client_write() call, and the time one poll() spends sending it
static constexpr size_t SEND_CHUNK_SIZE = 1024;
static constexpr uint32_t SEND_BUDGET_MS = 10;
static constexpr uint32_t ERROR_DURATION_MS = 1000;

void HttpRequestIDF::dump_config() {
  HttpRequestComponent::dump_config();
  ESP_LOGCONFIG(TAG,
                "  Buffer Size RX: %u\n"
                "  Buffer Size TX: %u\n"
                "  Custom CA Certificate: %s",
                this->buffer_size_rx_, this->buffer_size_tx_, YESNO(this->ca_certificate_ != nullptr));
}

esp_err_t HttpRequestIDF::http_event_handler(esp_http_client_event_t *evt) {
  auto *container = (HttpContainerIDF *) evt->user_data;

  switch (evt->event_id) {
    case HTTP_EVENT_ON_HEADER: {
      const std::string header_name = str_lower_case(evt->header_key);  // NOLINT
      if (should_collect_header(container->collect_headers_, header_name)) {
        const std::string header_value = evt->header_value;
        ESP_LOGD(TAG, "Received response header, name: %s, value: %s", header_name.c_str(), header_value.c_str());
        container->response_headers_.push_back({header_name, header_value});
      }
      break;
    }
    default: {
      break;
    }
  }
  return ESP_OK;
}

std::shared_ptr<HttpContainerIDF> HttpRequestIDF::create_container_(
    const char *url, const char *method, const std::vector<Header> &request_headers,
    const std::vector<std::string> &lower_case_collect_headers, uint32_t timeout_ms, [[maybe_unused]] bool async) {
  if (!network::is_connected()) {
    this->status_momentary_error(ERROR_DURATION_MS);
    ESP_LOGE(TAG, "HTTP Request failed; Not connected to network");
    return nullptr;
  }

  esp_http_client_method_t method_idf;
  if (strcmp(method, "GET") == 0) {
    method_idf = HTTP_METHOD_GET;
  } else if (strcmp(method, "POST") == 0) {
    method_idf = HTTP_METHOD_POST;
  } else if (strcmp(method, "PUT") == 0) {
    method_idf = HTTP_METHOD_PUT;
  } else if (strcmp(method, "DELETE") == 0) {
    method_idf = HTTP_METHOD_DELETE;
  } else if (strcmp(method, "PATCH") == 0) {
    method_idf = HTTP_METHOD_PATCH;
  } else {
    this->status_momentary_error(ERROR_DURATION_MS);
    ESP_LOGE(TAG, "HTTP Request failed; Unsupported method");
    return nullptr;
  }

  // Only the scheme counts; "https:" later in the URL (a query string) does not make it secure
  bool secure = ESPHOME_strncasecmp_P(url, ESPHOME_PSTR("https:"), 6) == 0;

  esp_http_client_config_t config = {};

  config.url = url;
  config.method = method_idf;
  config.timeout_ms = timeout_ms;
  config.disable_auto_redirect = !this->follow_redirects_;
  config.max_redirection_count = this->redirect_limit_;
  config.auth_type = HTTP_AUTH_TYPE_BASIC;
  if (secure && this->verify_ssl_) {
    if (this->ca_certificate_ != nullptr) {
      config.cert_pem = this->ca_certificate_;
#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
    } else {
      config.crt_bundle_attach = esp_crt_bundle_attach;
#endif
    }
  }

  if (this->useragent_ != nullptr) {
    config.user_agent = this->useragent_;
  }

  config.buffer_size = this->buffer_size_rx_;
  config.buffer_size_tx = this->buffer_size_tx_;

  config.event_handler = http_event_handler;
#ifdef USE_HTTP_REQUEST_IDF_SOCKET_POLL
  // Only TLS has a non-blocking connect worth having: the handshake. A plain connection waits for
  // the TCP connection either way.
  config.is_async = async && secure;
#endif

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (client == nullptr) {
    this->status_momentary_error(ERROR_DURATION_MS);
    ESP_LOGE(TAG, "HTTP Request failed; client could not be initialized");
    return nullptr;
  }

  std::shared_ptr<HttpContainerIDF> container = std::make_shared<HttpContainerIDF>(client);
  container->set_parent(this);
  container->set_watchdog_timeout(this->get_watchdog_timeout(timeout_ms));
  container->timeout_ms_ = timeout_ms;
  container->async_ = config.is_async;

  container->set_secure(secure);

  container->collect_headers_ = lower_case_collect_headers;
  esp_http_client_set_user_data(client, static_cast<void *>(container.get()));

  for (const auto &header : request_headers) {
    esp_http_client_set_header(client, header.name.c_str(), header.value.c_str());
  }

  return container;
}

std::shared_ptr<HttpContainer> HttpRequestIDF::perform(const char *url, const char *method, const std::string &body,
                                                       const std::vector<Header> &request_headers,
                                                       const std::vector<std::string> &lower_case_collect_headers,
                                                       uint32_t timeout_ms) {
  const uint32_t start = millis();
  std::shared_ptr<HttpContainerIDF> container =
      this->create_container_(url, method, request_headers, lower_case_collect_headers, timeout_ms, false);
  if (container == nullptr) {
    return nullptr;
  }
  watchdog::WatchdogManager wdm(container->get_watchdog_timeout());
  esp_http_client_handle_t client = container->client_;

  const int body_len = body.length();

  esp_err_t err = esp_http_client_open(client, body_len);
  if (err != ESP_OK) {
    this->status_momentary_error(ERROR_DURATION_MS);
    ESP_LOGE(TAG, "HTTP Request failed: %s", esp_err_to_name(err));
    esp_http_client_cleanup(client);
    return nullptr;
  }

  if (body_len > 0) {
    int write_left = body_len;
    int write_index = 0;
    const char *buf = body.c_str();
    while (write_left > 0) {
      int written = esp_http_client_write(client, buf + write_index, write_left);
      if (written <= 0) {
        err = ESP_FAIL;
        break;
      }
      write_left -= written;
      write_index += written;
      container->feed_wdt();
    }
  }

  if (err != ESP_OK) {
    this->status_momentary_error(ERROR_DURATION_MS);
    ESP_LOGE(TAG, "HTTP Request failed: %s", esp_err_to_name(err));
    esp_http_client_cleanup(client);
    return nullptr;
  }

  container->feed_wdt();
  // esp_http_client_fetch_headers() returns 0 for chunked transfer encoding (no Content-Length header).
  // The read() method handles content_length == 0 specially to support chunked responses.
  container->content_length = esp_http_client_fetch_headers(client);
  container->set_chunked(esp_http_client_is_chunked_response(client));
  container->feed_wdt();
  container->status_code = esp_http_client_get_status_code(client);
  container->feed_wdt();
  container->duration_ms = millis() - start;
  if (is_success(container->status_code)) {
    return container;
  }

  if (this->follow_redirects_) {
    auto num_redirects = this->redirect_limit_;
    while (is_redirect(container->status_code) && num_redirects > 0) {
      err = esp_http_client_set_redirection(client);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_http_client_set_redirection failed: %s", esp_err_to_name(err));
        this->status_momentary_error(ERROR_DURATION_MS);
        esp_http_client_cleanup(client);
        return nullptr;
      }
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
      char redirect_url[256]{};
      if (esp_http_client_get_url(client, redirect_url, sizeof(redirect_url) - 1) == ESP_OK) {
        ESP_LOGV(TAG, "redirecting to url: %s", redirect_url);
      }
#endif
      err = esp_http_client_open(client, 0);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_http_client_open failed: %s", esp_err_to_name(err));
        this->status_momentary_error(ERROR_DURATION_MS);
        esp_http_client_cleanup(client);
        return nullptr;
      }

      container->feed_wdt();
      // IDF is the only backend reusing the container across redirect hops;
      // drop the previous hop's headers (Arduino/host collect only the final response)
      container->response_headers_.clear();
      container->content_length = esp_http_client_fetch_headers(client);
      container->set_chunked(esp_http_client_is_chunked_response(client));
      container->feed_wdt();
      container->status_code = esp_http_client_get_status_code(client);
      container->feed_wdt();
      container->duration_ms = millis() - start;
      if (is_success(container->status_code)) {
        return container;
      }

      num_redirects--;
    }

    if (num_redirects == 0) {
      ESP_LOGW(TAG, "Reach redirect limit count=%d", this->redirect_limit_);
    }
  }

  ESP_LOGE(TAG, "HTTP Request failed; URL: %s; Code: %d", url, container->status_code);
  this->status_momentary_error(ERROR_DURATION_MS);
  return container;
}

std::shared_ptr<HttpContainer> HttpRequestIDF::begin(const char *url, const char *method, std::string &&body,
                                                     const std::vector<Header> &request_headers,
                                                     const std::vector<std::string> &lower_case_collect_headers,
                                                     uint32_t timeout_ms) {
  std::shared_ptr<HttpContainerIDF> container =
      this->create_container_(url, method, request_headers, lower_case_collect_headers, timeout_ms, true);
  if (container == nullptr) {
    return nullptr;
  }
  container->body_ = std::move(body);
  container->start_time_ = millis();
  container->progress_time_ = container->start_time_;
  container->redirects_left_ = this->follow_redirects_ ? this->redirect_limit_ : 0;
  container->stage_ = HttpContainerIDF::Stage::STAGE_CONNECT;
  return container;
}

HttpRequestIDF *HttpContainerIDF::idf_parent_() { return static_cast<HttpRequestIDF *>(this->parent_); }

HttpRequestProgress HttpContainerIDF::fail_(esp_err_t err) {
  ESP_LOGE(TAG, "HTTP Request failed: %s", esp_err_to_name(err));
  this->parent_->status_momentary_error(ERROR_DURATION_MS);
  this->stage_ = Stage::STAGE_FAILED;
  return HttpRequestProgress::HTTP_REQUEST_PROGRESS_FAILED;
}

HttpRequestProgress HttpContainerIDF::wait_() {
  // millis(), not the loop time: progress_time_ is set after calls that block within this pass
  if (millis() - this->progress_time_ >= this->timeout_ms_) {
    return this->fail_(ESP_ERR_TIMEOUT);
  }
  return HttpRequestProgress::HTTP_REQUEST_PROGRESS_PENDING;
}

bool HttpContainerIDF::socket_ready_(bool write) {
#ifdef USE_HTTP_REQUEST_IDF_SOCKET_POLL
  const int fd = esp_http_client_get_socket(this->client_);
  if (fd < 0) {
    return true;
  }
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(fd, &fds);
  timeval no_wait{};
  // An error also counts as ready, so that the call that follows reports it
  // Qualified: esphome::select is a namespace
  return ::select(fd + 1, write ? nullptr : &fds, write ? &fds : nullptr, nullptr, &no_wait) != 0;
#else
  return true;
#endif
}

// Each stage either finishes, or returns PENDING after a call that did not wait. The waits left are
// the name lookup and TCP connection in the first esp_http_client_open() of each hop: a non-blocking
// esp-tls connect that times out once never completes, as select() clears the sets it keeps.
HttpRequestProgress HttpContainerIDF::poll() {
  while (true) {
    switch (this->stage_) {
      case Stage::STAGE_DONE:
        return HttpRequestProgress::HTTP_REQUEST_PROGRESS_DONE;

      case Stage::STAGE_FAILED:
        return HttpRequestProgress::HTTP_REQUEST_PROGRESS_FAILED;

      case Stage::STAGE_CONNECT: {
        // Only the first call of a hop waits
        watchdog::WatchdogManager wdm(this->connecting_ ? 0 : this->get_watchdog_timeout());
        esp_http_client_set_timeout_ms(this->client_, this->connecting_ ? 1 : this->timeout_ms_);
        this->feed_wdt();
        esp_err_t err = esp_http_client_open(this->client_, this->body_.size());
        this->feed_wdt();
        if (err == ESP_ERR_HTTP_EAGAIN) {
          this->connecting_ = true;
          return this->wait_();
        }
        this->connecting_ = false;
        if (err != ESP_OK) {
          return this->fail_(err);
        }
#ifdef USE_HTTP_REQUEST_IDF_SOCKET_POLL
        if (this->async_) {
          // Back to a blocking socket, so that mbedtls waits for the rest of a record instead of
          // failing; reads only start once the socket is readable
          const int fd = esp_http_client_get_socket(this->client_);
          if (fd >= 0) {
            const int flags = fcntl(fd, F_GETFL, 0);
            if (flags < 0 || fcntl(fd, F_SETFL, flags & ~O_NONBLOCK) < 0) {
              ESP_LOGE(TAG, "Could not make the socket blocking: errno %d", errno);
              return this->fail_(ESP_FAIL);
            }
          }
        }
#endif
        esp_http_client_set_timeout_ms(this->client_, this->timeout_ms_);
        this->progress_time_ = millis();
        this->stage_ = this->body_sent_ < this->body_.size() ? Stage::STAGE_SEND_BODY : Stage::STAGE_HEADERS;
        break;
      }

      case Stage::STAGE_SEND_BODY: {
        // esp_http_client_write() returns only once all it was given is sent, so give it a piece at a time
        watchdog::WatchdogManager wdm(this->get_watchdog_timeout());
        const uint32_t start = millis();
        while (this->body_sent_ < this->body_.size()) {
          if (!this->socket_ready_(true)) {
            return this->wait_();
          }
          const size_t len = std::min(this->body_.size() - this->body_sent_, SEND_CHUNK_SIZE);
          int written = esp_http_client_write(this->client_, this->body_.data() + this->body_sent_, len);
          this->feed_wdt();
          if (written < 0) {
            return this->fail_(ESP_FAIL);
          }
          if (written == 0) {
            return this->wait_();
          }
          this->body_sent_ += written;
          this->progress_time_ = millis();
          if (this->progress_time_ - start >= SEND_BUDGET_MS && this->body_sent_ < this->body_.size()) {
            return HttpRequestProgress::HTTP_REQUEST_PROGRESS_PENDING;
          }
        }
        this->body_ = std::string();
        this->stage_ = Stage::STAGE_HEADERS;
        break;
      }

      case Stage::STAGE_HEADERS: {
        if (!this->socket_ready_(false)) {
          if (millis() - this->progress_time_ < this->timeout_ms_) {
            return HttpRequestProgress::HTTP_REQUEST_PROGRESS_PENDING;
          }
          // As perform(), where esp_http_client_fetch_headers() times out: no response is status -1
          return this->response_failed_(-1);
        }
        // The response has begun to arrive, so this waits only for the rest of the headers, or over TLS
        // the rest of the record that holds them
        watchdog::WatchdogManager wdm(this->get_watchdog_timeout());
        this->feed_wdt();
        // esp_http_client_fetch_headers() returns 0 for chunked transfer encoding (no Content-Length header)
        const int64_t length = esp_http_client_fetch_headers(this->client_);
        this->feed_wdt();
        this->duration_ms = millis() - this->start_time_;
        if (length < 0) {
          return this->response_failed_(-1);
        }
        this->content_length = length;
        this->set_chunked(esp_http_client_is_chunked_response(this->client_));
        this->status_code = esp_http_client_get_status_code(this->client_);
        if (is_success(this->status_code)) {
          this->stage_ = Stage::STAGE_DONE;
          break;
        }
        if (this->redirects_left_ > 0 && is_redirect(this->status_code)) {
          esp_err_t err = esp_http_client_set_redirection(this->client_);
          if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_http_client_set_redirection failed: %s", esp_err_to_name(err));
            return this->fail_(err);
          }
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
          char redirect_url[256]{};
          if (esp_http_client_get_url(this->client_, redirect_url, sizeof(redirect_url) - 1) == ESP_OK) {
            ESP_LOGV(TAG, "redirecting to url: %s", redirect_url);
          }
#endif
          this->redirects_left_--;
          // Collect only the final response's headers; redirects do not resend the body
          this->response_headers_.clear();
          this->body_ = std::string();
          this->body_sent_ = 0;
          this->progress_time_ = millis();
          this->stage_ = Stage::STAGE_CONNECT;
          break;
        }
        if (is_redirect(this->status_code) && this->idf_parent_()->follow_redirects_) {
          ESP_LOGW(TAG, "Reach redirect limit count=%d", this->idf_parent_()->redirect_limit_);
        }
        return this->response_failed_(this->status_code);
      }
    }
  }
}

HttpRequestProgress HttpContainerIDF::response_failed_(int status_code) {
  this->status_code = status_code;
  this->duration_ms = millis() - this->start_time_;
  char url[256]{};
  esp_http_client_get_url(this->client_, url, sizeof(url) - 1);
  ESP_LOGE(TAG, "HTTP Request failed; URL: %s; Code: %d", url, this->status_code);
  this->parent_->status_momentary_error(ERROR_DURATION_MS);
  this->stage_ = Stage::STAGE_DONE;
  return HttpRequestProgress::HTTP_REQUEST_PROGRESS_DONE;
}

bool HttpContainerIDF::is_read_complete() const {
  // Base class handles no-body status codes and non-chunked content_length completion
  if (HttpContainer::is_read_complete()) {
    return true;
  }
  // For chunked responses, use the authoritative ESP-IDF completion check
  return this->is_chunked_ && esp_http_client_is_complete_data_received(this->client_);
}

// ESP-IDF HTTP read implementation (blocking mode)
//
// WARNING: Return values differ from BSD sockets! See http_request.h for full documentation.
//
// esp_http_client_read() in blocking mode returns:
//   > 0: bytes read
//   0: all chunked data received (is_chunk_complete true) or connection closed
//   -ESP_ERR_HTTP_EAGAIN: transport timeout, no data available yet
//   < 0: error
//
// We normalize to HttpContainer::read() contract:
//   > 0: bytes read
//   0: all content read (for both content_length-based and chunked completion)
//   < 0: error/connection closed
//
// Note on chunked transfer encoding:
//   esp_http_client_fetch_headers() returns 0 for chunked responses (no Content-Length header).
//   When esp_http_client_read() returns 0 for a chunked response, is_read_complete() calls
//   esp_http_client_is_complete_data_received() to distinguish successful completion from
//   connection errors. Callers use http_read_loop_result() which checks is_read_complete()
//   to return COMPLETE for successful chunked EOF.
//
// Streaming chunked responses are not supported (see http_request.h for details).
// When data stops arriving, esp_http_client_read() returns -ESP_ERR_HTTP_EAGAIN
// after its internal transport timeout (configured via timeout_ms) expires.
// This is passed through as a negative return value, which callers treat as an error.
int HttpContainerIDF::read(uint8_t *buf, size_t max_len) {
  watchdog::WatchdogManager wdm(this->get_watchdog_timeout());
  esp_http_client_set_timeout_ms(this->client_, this->timeout_ms_);
  return this->read_(buf, max_len);
}

int HttpContainerIDF::read_available(uint8_t *buf, size_t max_len) {
  // A zero timeout only polls; TLS still waits for the rest of a record that has begun to arrive
  watchdog::WatchdogManager wdm(this->get_watchdog_timeout());
  esp_http_client_set_timeout_ms(this->client_, 0);
  int read_len_or_error = this->read_(buf, max_len);
  return read_len_or_error == -ESP_ERR_HTTP_EAGAIN ? 0 : read_len_or_error;
}

int HttpContainerIDF::read_(uint8_t *buf, size_t max_len) {
  const uint32_t start = millis();

  // Check if we've already read all expected content (non-chunked and no-body only).
  // Use the base class check here, NOT the override: esp_http_client_is_complete_data_received()
  // returns true as soon as all data arrives from the network, but data may still be in
  // the client's internal buffer waiting to be consumed by esp_http_client_read().
  if (HttpContainer::is_read_complete()) {
    return 0;  // All content read successfully
  }

  this->feed_wdt();
  int read_len_or_error = esp_http_client_read(this->client_, (char *) buf, max_len);
  this->feed_wdt();

  this->duration_ms += (millis() - start);

  if (read_len_or_error > 0) {
    this->bytes_read_ += read_len_or_error;
    return read_len_or_error;
  }

  // esp_http_client_read() returns 0 when:
  // - Known content_length: connection closed before all data received (error)
  // - Chunked encoding: all chunks received (is_chunk_complete true, genuine EOF)
  //
  // Return 0 in both cases. Callers use http_read_loop_result() which calls
  // is_read_complete() to distinguish these:
  // - Chunked complete: is_read_complete() returns true (via
  //   esp_http_client_is_complete_data_received()), caller gets COMPLETE
  // - Non-chunked incomplete: is_read_complete() returns false, caller
  //   eventually gets TIMEOUT (since no more data arrives)
  if (read_len_or_error == 0) {
    return 0;
  }

  // Negative value - error, return the actual error code for debugging
  return read_len_or_error;
}

void HttpContainerIDF::end() {
  if (this->client_ == nullptr) {
    return;  // Already cleaned up
  }
  watchdog::WatchdogManager wdm(this->get_watchdog_timeout());

  esp_http_client_close(this->client_);
  esp_http_client_cleanup(this->client_);
  this->client_ = nullptr;
}

void HttpContainerIDF::feed_wdt() {
  // Tests to see if the executing task has a watchdog timer attached
  if (esp_task_wdt_status(nullptr) == ESP_OK) {
    App.feed_wdt();
  }
}

}  // namespace esphome::http_request

#endif  // USE_ESP32
