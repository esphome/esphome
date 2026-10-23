#pragma once

#include <list>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "esphome/components/json/json_util.h"
#include "esphome/core/application.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/alloc_helpers.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::http_request {

struct Header {
  std::string name;
  std::string value;
};

/// One request header of an action; codegen puts these in a shared flash table.
template<typename... Ts> struct RequestHeader {
  const char *name;
  const char *(*value)(Ts...);
};

// Some common HTTP status codes
enum HttpStatus {
  HTTP_STATUS_OK = 200,
  HTTP_STATUS_NO_CONTENT = 204,
  HTTP_STATUS_RESET_CONTENT = 205,
  HTTP_STATUS_PARTIAL_CONTENT = 206,

  /* 3xx - Redirection */
  HTTP_STATUS_MULTIPLE_CHOICES = 300,
  HTTP_STATUS_MOVED_PERMANENTLY = 301,
  HTTP_STATUS_FOUND = 302,
  HTTP_STATUS_SEE_OTHER = 303,
  HTTP_STATUS_NOT_MODIFIED = 304,
  HTTP_STATUS_TEMPORARY_REDIRECT = 307,
  HTTP_STATUS_PERMANENT_REDIRECT = 308,

  /* 4XX - CLIENT ERROR */
  HTTP_STATUS_BAD_REQUEST = 400,
  HTTP_STATUS_UNAUTHORIZED = 401,
  HTTP_STATUS_FORBIDDEN = 403,
  HTTP_STATUS_NOT_FOUND = 404,
  HTTP_STATUS_METHOD_NOT_ALLOWED = 405,
  HTTP_STATUS_NOT_ACCEPTABLE = 406,
  HTTP_STATUS_LENGTH_REQUIRED = 411,

  /* 5xx - Server Error */
  HTTP_STATUS_INTERNAL_ERROR = 500
};

/**
 * @brief Returns true if the HTTP status code is a redirect.
 *
 * @param status the HTTP status code to check
 * @return true if the status code is a redirect, false otherwise
 */
inline bool is_redirect(int const status) {
  switch (status) {
    case HTTP_STATUS_MOVED_PERMANENTLY:
    case HTTP_STATUS_FOUND:
    case HTTP_STATUS_SEE_OTHER:
    case HTTP_STATUS_TEMPORARY_REDIRECT:
    case HTTP_STATUS_PERMANENT_REDIRECT:
      return true;
    default:
      return false;
  }
}

/**
 * @brief Checks if the given HTTP status code indicates a successful request.
 *
 * A successful request is one where the status code is in the range 200-299
 *
 * @param status the HTTP status code to check
 * @return true if the status code indicates a successful request, false otherwise
 */
inline bool is_success(int const status) { return status >= HTTP_STATUS_OK && status < HTTP_STATUS_MULTIPLE_CHOICES; }

/// Check if a header name should be collected (linear scan, fine for small lists)
inline bool should_collect_header(const std::vector<std::string> &lower_case_collect_headers,
                                  const std::string &lower_header_name) {
  for (const auto &h : lower_case_collect_headers) {
    if (h == lower_header_name)
      return true;
  }
  return false;
}

/*
 * HTTP Container Read Semantics
 * =============================
 *
 * IMPORTANT: These semantics differ from standard BSD sockets!
 *
 * BSD socket read() returns:
 *   > 0: bytes read
 *   == 0: connection closed (EOF)
 *   < 0: error (check errno)
 *
 * HttpContainer::read() returns:
 *   > 0: bytes read successfully
 *   == 0: no data available yet OR all content read
 *         (caller should check bytes_read vs content_length)
 *   < 0: error or connection closed (caller should EXIT)
 *        HTTP_ERROR_CONNECTION_CLOSED (-1) = connection closed prematurely
 *        other negative values = platform-specific errors
 *
 * Platform behaviors:
 *   - ESP-IDF: blocking reads, 0 only returned when all content read
 *   - Arduino: non-blocking, 0 means "no data yet" or "all content read"
 *
 * Chunked responses that complete in a reasonable time work correctly on both
 * platforms. The limitation below applies only to *streaming* chunked
 * responses where data arrives slowly over a long period.
 *
 * Streaming chunked responses are NOT supported (all platforms):
 *   The read helpers (http_read_loop_result, http_read_fully) block the main
 *   event loop until all response data is received. For streaming responses
 *   where data trickles in slowly (e.g., TTS streaming via ffmpeg proxy),
 *   this starves the event loop on both ESP-IDF and Arduino. If data arrives
 *   just often enough to avoid the caller's timeout, the loop runs
 *   indefinitely. If data stops entirely, ESP-IDF fails with
 *   -ESP_ERR_HTTP_EAGAIN (transport timeout) while Arduino spins with
 *   delay(1) until the caller's timeout fires. Supporting streaming requires
 *   a non-blocking incremental read pattern that yields back to the event
 *   loop between chunks. Components that need streaming should use
 *   esp_http_client directly on a separate FreeRTOS task with
 *   esp_http_client_is_complete_data_received() for completion detection
 *   (see audio_reader.cpp for an example).
 *
 * Chunked transfer encoding - platform differences:
 *   - ESP-IDF HttpContainer:
 *       HttpContainerIDF overrides is_read_complete() to call
 *       esp_http_client_is_complete_data_received(), which is the
 *       authoritative completion check for both chunked and non-chunked
 *       transfers. When esp_http_client_read() returns 0 for a completed
 *       chunked response, read() returns 0 and is_read_complete() returns
 *       true, so callers get COMPLETE from http_read_loop_result().
 *
 *   - Arduino HttpContainer:
 *       Chunked responses are decoded internally (see
 *       HttpContainerArduino::read_chunked_()). When the final chunk arrives,
 *       is_chunked_ is cleared and content_length is set to bytes_read_.
 *       Completion is then detected via is_read_complete(), and a subsequent
 *       read() returns 0 to indicate "all content read" (not
 *       HTTP_ERROR_CONNECTION_CLOSED).
 *
 * Use the helper functions below instead of checking return values directly:
 *   - http_read_loop_result(): for manual loops with per-chunk processing
 *   - http_read_fully(): for simple "read N bytes into buffer" operations
 */

/// Error code returned by HttpContainer::read() when connection closed prematurely
/// NOTE: Unlike BSD sockets where 0 means EOF, here 0 means "no data yet, retry"
static constexpr int HTTP_ERROR_CONNECTION_CLOSED = -1;

/// Status of a read operation
enum class HttpReadStatus : uint8_t {
  OK,       ///< Read completed successfully
  ERROR,    ///< Read error occurred
  TIMEOUT,  ///< Timeout waiting for data
};

/// Result of an HTTP read operation
struct HttpReadResult {
  HttpReadStatus status;  ///< Status of the read operation
  int error_code;         ///< Error code from read() on failure, 0 on success
};

/// Result of processing a non-blocking read with timeout (for manual loops)
enum class HttpReadLoopResult : uint8_t {
  DATA,      ///< Data was read, process it
  COMPLETE,  ///< All content has been read, caller should exit loop
  RETRY,     ///< No data yet, already delayed, caller should continue loop
  ERROR,     ///< Read error, caller should exit loop
  TIMEOUT,   ///< Timeout waiting for data, caller should exit loop
};

/// Progress of a request started with HttpRequestComponent::begin()
enum class HttpRequestProgress : uint8_t {
  HTTP_REQUEST_PROGRESS_PENDING,  ///< Still connecting, sending, or waiting for the response headers
  HTTP_REQUEST_PROGRESS_DONE,     ///< Response headers received; status_code is set and the body can be read
  HTTP_REQUEST_PROGRESS_FAILED,   ///< No response; the container must still be ended
};

/// Process a read result with timeout tracking and delay handling
/// @param bytes_read_or_error Return value from read() - positive for bytes read, negative for error
/// @param last_data_time Time of last successful read, updated when data received
/// @param timeout_ms Maximum time to wait for data
/// @param is_read_complete Whether all expected content has been read (from HttpContainer::is_read_complete())
/// @return How the caller should proceed - see HttpReadLoopResult enum
inline HttpReadLoopResult http_read_loop_result(int bytes_read_or_error, uint32_t &last_data_time, uint32_t timeout_ms,
                                                bool is_read_complete) {
  if (bytes_read_or_error > 0) {
    last_data_time = millis();
    return HttpReadLoopResult::DATA;
  }
  if (bytes_read_or_error < 0) {
    return HttpReadLoopResult::ERROR;
  }
  // bytes_read_or_error == 0: either "no data yet" or "all content read"
  if (is_read_complete) {
    return HttpReadLoopResult::COMPLETE;
  }
  if (millis() - last_data_time >= timeout_ms) {
    return HttpReadLoopResult::TIMEOUT;
  }
  delay(1);  // Small delay to prevent tight spinning
  return HttpReadLoopResult::RETRY;
}

class HttpRequestComponent;

class HttpContainer : public Parented<HttpRequestComponent> {
 public:
  virtual ~HttpContainer() = default;
  size_t content_length{0};
  int status_code{-1};  ///< -1 indicates no response received yet
  uint32_t duration_ms{0};

  /**
   * @brief Read data from the HTTP response body.
   *
   * WARNING: These semantics differ from BSD sockets!
   * BSD sockets: 0 = EOF (connection closed)
   * This method: 0 = no data yet OR all content read, negative = error/closed
   *
   * @param buf Buffer to read data into
   * @param max_len Maximum number of bytes to read
   * @return
   *   - > 0: Number of bytes read successfully
   *   - 0: No data available yet OR all content read
   *        (check get_bytes_read() >= content_length to distinguish)
   *   - HTTP_ERROR_CONNECTION_CLOSED (-1): Connection closed prematurely
   *   - < -1: Other error (platform-specific error code)
   *
   * Platform notes:
   *   - ESP-IDF: blocking read, 0 only when all content read
   *   - Arduino: non-blocking, 0 can mean "no data yet" or "all content read"
   *
   * Use get_bytes_read() and content_length to track progress.
   * When get_bytes_read() >= content_length, all data has been received.
   *
   * IMPORTANT: Do not use raw return values directly. Use these helpers:
   *   - http_read_loop_result(): for loops with per-chunk processing
   *   - http_read_fully(): for simple "read N bytes" operations
   */
  virtual int read(uint8_t *buf, size_t max_len) = 0;
  /// Like read(), but returns 0 at once when no data has arrived yet, where read() may wait for it.
  virtual int read_available(uint8_t *buf, size_t max_len) { return this->read(buf, max_len); }
  virtual void end() = 0;

  /// Advance a request started with HttpRequestComponent::begin(); call from the main loop until it is no
  /// longer pending. Containers from start() are already done. Most calls return quickly, but on ESP-IDF the
  /// first call for each host waits for the name lookup and the TCP connection, up to the whole timeout.
  virtual HttpRequestProgress poll() { return HttpRequestProgress::HTTP_REQUEST_PROGRESS_DONE; }

  void set_secure(bool secure) { this->secure_ = secure; }
  void set_watchdog_timeout(uint32_t watchdog_timeout) { this->watchdog_timeout_ = watchdog_timeout; }
  uint32_t get_watchdog_timeout() const { return this->watchdog_timeout_; }
  void set_chunked(bool chunked) { this->is_chunked_ = chunked; }

  size_t get_bytes_read() const { return this->bytes_read_; }

  /// Check if all expected content has been read.
  /// Base implementation handles non-chunked responses and status-code-based no-body checks.
  /// Platform implementations may override for chunked completion detection:
  ///   - ESP-IDF: overrides to call esp_http_client_is_complete_data_received() for chunked.
  ///   - Arduino: read_chunked_() clears is_chunked_ and sets content_length on the final
  ///     chunk, after which the base implementation detects completion.
  virtual bool is_read_complete() const {
    // Per RFC 9112, these responses have no body:
    // - 1xx (Informational), 204 No Content, 205 Reset Content, 304 Not Modified
    if ((this->status_code >= 100 && this->status_code < 200) || this->status_code == HTTP_STATUS_NO_CONTENT ||
        this->status_code == HTTP_STATUS_RESET_CONTENT || this->status_code == HTTP_STATUS_NOT_MODIFIED) {
      return true;
    }
    // For non-chunked responses, complete when bytes_read >= content_length
    // This handles both Content-Length: 0 and Content-Length: N cases
    return !this->is_chunked_ && this->bytes_read_ >= this->content_length;
  }

  /**
   * @brief Get the value of a response header by name.
   * @param header_name The name of the header to retrieve.
   * @return The value of the header if found, otherwise an empty string.
   */
  std::string get_response_header(const std::string &header_name);

 protected:
  size_t bytes_read_{0};
  uint32_t watchdog_timeout_{0};
  bool secure_{false};
  bool is_chunked_{false};  ///< True if response uses chunked transfer encoding
  std::vector<Header> response_headers_{};
};

/// Read data from HTTP container into buffer with timeout handling
/// Handles feed_wdt, yield, and timeout checking internally
/// @param container The HTTP container to read from
/// @param buffer Buffer to read into
/// @param total_size Total bytes to read
/// @param chunk_size Maximum bytes per read call
/// @param timeout_ms Read timeout in milliseconds
/// @return HttpReadResult with status and error_code on failure; use container->get_bytes_read() for total bytes read
inline HttpReadResult http_read_fully(HttpContainer *container, uint8_t *buffer, size_t total_size, size_t chunk_size,
                                      uint32_t timeout_ms) {
  size_t read_index = 0;
  uint32_t last_data_time = millis();

  while (read_index < total_size) {
    int read_bytes_or_error = container->read(buffer + read_index, std::min(chunk_size, total_size - read_index));

    App.feed_wdt();
    yield();

    auto result = http_read_loop_result(read_bytes_or_error, last_data_time, timeout_ms, container->is_read_complete());
    if (result == HttpReadLoopResult::RETRY)
      continue;
    if (result == HttpReadLoopResult::COMPLETE)
      break;  // Server sent less data than requested, but transfer is complete
    if (result == HttpReadLoopResult::ERROR)
      return {HttpReadStatus::ERROR, read_bytes_or_error};
    if (result == HttpReadLoopResult::TIMEOUT)
      return {HttpReadStatus::TIMEOUT, 0};

    read_index += read_bytes_or_error;
  }
  return {HttpReadStatus::OK, 0};
}

class HttpRequestResponseTrigger final : public Trigger<std::shared_ptr<HttpContainer>, std::string &> {
 public:
  void process(const std::shared_ptr<HttpContainer> &container, std::string &response_body) {
    this->trigger(container, response_body);
  }
};

class HttpRequestComponent : public Component {
 public:
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void set_useragent(const char *useragent) { this->useragent_ = useragent; }
  void set_timeout(uint32_t timeout) { this->timeout_ = timeout; }
  uint32_t get_timeout() const { return this->timeout_; }
  void set_watchdog_timeout(uint32_t watchdog_timeout) { this->watchdog_timeout_ = watchdog_timeout; }
  uint32_t get_watchdog_timeout() const { return this->watchdog_timeout_; }
  uint32_t get_watchdog_timeout(uint32_t timeout_ms) const {
    if (this->watchdog_timeout_ == 0 || timeout_ms <= this->timeout_)
      return this->watchdog_timeout_;
    return this->watchdog_timeout_ + 3 * (timeout_ms - this->timeout_);
  }
  void set_follow_redirects(bool follow_redirects) { this->follow_redirects_ = follow_redirects; }
  void set_redirect_limit(uint16_t limit) { this->redirect_limit_ = limit; }

  std::shared_ptr<HttpContainer> get(const char *url) { return this->start(url, "GET", "", std::vector<Header>{}); }
  std::shared_ptr<HttpContainer> get(const char *url, const std::vector<Header> &request_headers) {
    return this->start(url, "GET", "", request_headers);
  }
  std::shared_ptr<HttpContainer> get(const char *url, const std::vector<Header> &request_headers,
                                     const std::vector<std::string> &lower_case_collect_headers) {
    return this->start(url, "GET", "", request_headers, lower_case_collect_headers);
  }
  std::shared_ptr<HttpContainer> post(const char *url, const std::string &body) {
    return this->start(url, "POST", body, std::vector<Header>{});
  }
  std::shared_ptr<HttpContainer> post(const char *url, const std::string &body,
                                      const std::vector<Header> &request_headers) {
    return this->start(url, "POST", body, request_headers);
  }
  std::shared_ptr<HttpContainer> post(const char *url, const std::string &body,
                                      const std::vector<Header> &request_headers,
                                      const std::vector<std::string> &lower_case_collect_headers) {
    return this->start(url, "POST", body, request_headers, lower_case_collect_headers);
  }

  std::shared_ptr<HttpContainer> get(const std::string &url) { return this->get(url.c_str()); }
  std::shared_ptr<HttpContainer> get(const std::string &url, const std::vector<Header> &request_headers) {
    return this->get(url.c_str(), request_headers);
  }
  std::shared_ptr<HttpContainer> get(const std::string &url, const std::vector<Header> &request_headers,
                                     const std::vector<std::string> &lower_case_collect_headers) {
    return this->get(url.c_str(), request_headers, lower_case_collect_headers);
  }
  std::shared_ptr<HttpContainer> post(const std::string &url, const std::string &body) {
    return this->post(url.c_str(), body);
  }
  std::shared_ptr<HttpContainer> post(const std::string &url, const std::string &body,
                                      const std::vector<Header> &request_headers) {
    return this->post(url.c_str(), body, request_headers);
  }
  std::shared_ptr<HttpContainer> post(const std::string &url, const std::string &body,
                                      const std::vector<Header> &request_headers,
                                      const std::vector<std::string> &lower_case_collect_headers) {
    return this->post(url.c_str(), body, request_headers, lower_case_collect_headers);
  }

  // Remove before 2027.1.0
  ESPDEPRECATED("Pass request_headers as std::vector<Header> instead of std::list. Removed in 2027.1.0.", "2026.7.0")
  std::shared_ptr<HttpContainer> get(const std::string &url, const std::list<Header> &request_headers) {
    return this->get(url, std::vector<Header>(request_headers.begin(), request_headers.end()));
  }
  // Remove before 2027.1.0
  ESPDEPRECATED("Pass request_headers as std::vector<Header> instead of std::list. Removed in 2027.1.0.", "2026.7.0")
  std::shared_ptr<HttpContainer> get(const std::string &url, const std::list<Header> &request_headers,
                                     const std::vector<std::string> &collect_headers) {
    return this->get(url, std::vector<Header>(request_headers.begin(), request_headers.end()), collect_headers);
  }
  // Remove before 2027.1.0
  ESPDEPRECATED("Pass request_headers as std::vector<Header> instead of std::list. Removed in 2027.1.0.", "2026.7.0")
  std::shared_ptr<HttpContainer> post(const std::string &url, const std::string &body,
                                      const std::list<Header> &request_headers) {
    return this->post(url, body, std::vector<Header>(request_headers.begin(), request_headers.end()));
  }
  // Remove before 2027.1.0
  ESPDEPRECATED("Pass request_headers as std::vector<Header> instead of std::list. Removed in 2027.1.0.", "2026.7.0")
  std::shared_ptr<HttpContainer> post(const std::string &url, const std::string &body,
                                      const std::list<Header> &request_headers,
                                      const std::vector<std::string> &collect_headers) {
    return this->post(url, body, std::vector<Header>(request_headers.begin(), request_headers.end()), collect_headers);
  }

  std::shared_ptr<HttpContainer> start(const char *url, const char *method, const std::string &body,
                                       const std::vector<Header> &request_headers) {
    // Call perform() directly to avoid ambiguity with the deprecated overloads
    return this->perform(url, method, body, request_headers, {}, this->timeout_);
  }
  std::shared_ptr<HttpContainer> start(const std::string &url, const std::string &method, const std::string &body,
                                       const std::vector<Header> &request_headers) {
    return this->start(url.c_str(), method.c_str(), body, request_headers);
  }

  // Remove before 2027.1.0
  ESPDEPRECATED("Pass request_headers as std::vector<Header> instead of std::list. Removed in 2027.1.0.", "2026.7.0")
  std::shared_ptr<HttpContainer> start(const std::string &url, const std::string &method, const std::string &body,
                                       const std::list<Header> &request_headers) {
    return this->start(url, method, body, std::vector<Header>(request_headers.begin(), request_headers.end()));
  }

  // Remove before 2027.1.0
  ESPDEPRECATED("Pass collect_headers as std::vector<std::string> instead of std::set. Removed in 2027.1.0.",
                "2026.7.0")
  std::shared_ptr<HttpContainer> start(const std::string &url, const std::string &method, const std::string &body,
                                       const std::vector<Header> &request_headers,
                                       const std::set<std::string> &collect_headers) {
    std::vector<std::string> lower;
    lower.reserve(collect_headers.size());
    for (const auto &h : collect_headers) {
      lower.push_back(str_lower_case(h));  // NOLINT
    }
    return this->perform(url.c_str(), method.c_str(), body, request_headers, lower, this->timeout_);
  }

  // Remove before 2027.1.0
  ESPDEPRECATED("Pass request_headers as std::vector<Header> instead of std::list, and collect_headers as "
                "std::vector<std::string> instead of std::set. Removed in 2027.1.0.",
                "2026.7.0")
  std::shared_ptr<HttpContainer> start(const std::string &url, const std::string &method, const std::string &body,
                                       const std::list<Header> &request_headers,
                                       const std::set<std::string> &collect_headers) {
    std::vector<std::string> lower;
    lower.reserve(collect_headers.size());
    for (const auto &h : collect_headers) {
      lower.push_back(str_lower_case(h));  // NOLINT
    }
    return this->perform(url.c_str(), method.c_str(), body,
                         std::vector<Header>(request_headers.begin(), request_headers.end()), lower, this->timeout_);
  }

  // Remove before 2027.1.0
  ESPDEPRECATED("Pass request_headers as std::vector<Header> instead of std::list. Removed in 2027.1.0.", "2026.7.0")
  std::shared_ptr<HttpContainer> start(const std::string &url, const std::string &method, const std::string &body,
                                       const std::list<Header> &request_headers,
                                       const std::vector<std::string> &lower_case_collect_headers) {
    return this->perform(url.c_str(), method.c_str(), body,
                         std::vector<Header>(request_headers.begin(), request_headers.end()),
                         lower_case_collect_headers, this->timeout_);
  }

  std::shared_ptr<HttpContainer> start(const char *url, const char *method, const std::string &body,
                                       const std::vector<Header> &request_headers,
                                       const std::vector<std::string> &lower_case_collect_headers) {
    return this->perform(url, method, body, request_headers, lower_case_collect_headers, this->timeout_);
  }
  std::shared_ptr<HttpContainer> start(const char *url, const char *method, const std::string &body,
                                       const std::vector<Header> &request_headers,
                                       const std::vector<std::string> &lower_case_collect_headers,
                                       uint32_t timeout_ms) {
    return this->perform(url, method, body, request_headers, lower_case_collect_headers, timeout_ms);
  }
  std::shared_ptr<HttpContainer> start(const std::string &url, const std::string &method, const std::string &body,
                                       const std::vector<Header> &request_headers,
                                       const std::vector<std::string> &lower_case_collect_headers) {
    return this->start(url.c_str(), method.c_str(), body, request_headers, lower_case_collect_headers);
  }

  /// Start a request without waiting for the response, then poll() the container until it is not pending.
  /// Returns nullptr when the request cannot be started. Platforms without a non-blocking client complete
  /// the request here.
  virtual std::shared_ptr<HttpContainer> begin(const char *url, const char *method, std::string &&body,
                                               const std::vector<Header> &request_headers,
                                               const std::vector<std::string> &lower_case_collect_headers,
                                               uint32_t timeout_ms) {
    return this->perform(url, method, body, request_headers, lower_case_collect_headers, timeout_ms);
  }

 protected:
  virtual std::shared_ptr<HttpContainer> perform(const char *url, const char *method, const std::string &body,
                                                 const std::vector<Header> &request_headers,
                                                 const std::vector<std::string> &lower_case_collect_headers,
                                                 uint32_t timeout_ms) = 0;
  const char *useragent_{nullptr};
  bool follow_redirects_{};
  uint16_t redirect_limit_{};
  uint32_t timeout_{4500};
  uint32_t watchdog_timeout_{0};
};

/// Shared by every HttpRequestSendAction, so the message is not repeated in each template
void log_request_dropped();

template<typename... Ts> class HttpRequestSendAction final : public Action<Ts...>, public Component {
 public:
  HttpRequestSendAction(HttpRequestComponent *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, url)
  TEMPLATABLE_VALUE(const char *, method)
  TEMPLATABLE_VALUE(std::string, body)
  TEMPLATABLE_VALUE(uint32_t, timeout)
#ifdef USE_HTTP_REQUEST_RESPONSE
  TEMPLATABLE_VALUE(bool, capture_response)
#endif

  static_assert(std::is_trivially_copyable_v<RequestHeader<Ts...>> &&
                    sizeof(RequestHeader<Ts...>) % sizeof(uint32_t) == 0,
                "ESP8266 reads request headers from flash in whole words");
  /// Codegen only: the table must outlive the action.
  void set_request_headers(const RequestHeader<Ts...> *headers, size_t count) {
    this->request_headers_ = ConstVector<RequestHeader<Ts...>>(headers, count);
  }

  void add_collect_header(const char *value) { this->lower_case_collect_headers_.emplace_back(value); }

  void init_json(size_t count) { this->json_.init(count); }
  void add_json(const char *key, TemplatableValue<std::string, Ts...> value) { this->json_.push_back({key, value}); }

  void set_json(std::function<void(Ts..., JsonObject)> json_func) { this->json_func_ = json_func; }

#ifdef USE_HTTP_REQUEST_RESPONSE
  Trigger<std::shared_ptr<HttpContainer>, std::string &, Ts...> *get_success_trigger_with_response() {
    return &this->success_trigger_with_response_;
  }
#endif
  Trigger<std::shared_ptr<HttpContainer>, Ts...> *get_success_trigger() { return &this->success_trigger_; }

  Trigger<Ts...> *get_error_trigger() { return &this->error_trigger_; }

  void set_max_response_buffer_size(size_t max_response_buffer_size) {
    this->max_response_buffer_size_ = max_response_buffer_size;
  }

  void setup() override {
    // play_complex() may already have run from an on_boot trigger at the same priority
    if (this->num_running_ == 0)
      this->disable_loop();
  }

  void play_complex(const Ts &...x) override {
    this->num_running_++;
    if (this->active_ < MAX_ACTIVE && this->queued_ == 0) {
      // Started outside pending_: the request's lambdas can stop this action, which clears pending_
      std::list<PendingRequest> request;
      request.emplace_back(x...);
      this->active_++;
      const uint8_t generation = this->stop_generation_;
      const bool started = this->start_(request.front());
      if (generation != this->stop_generation_) {
        // stop() has already reset the counts and the run
        this->end_all_(request);
        return;
      }
      if (!started) {
        // Not started; report it at once, as the loop may not run again
        this->active_--;
        this->report_error_(x...);
        return;
      }
      this->pending_.splice(this->pending_.end(), request);
    } else if (this->queued_ < MAX_QUEUED) {
      this->queued_++;
      this->pending_.emplace_back(x...);
    } else {
      log_request_dropped();
      this->report_error_(x...);
      return;
    }
    this->enable_loop();
  }

  void loop() override {
    if (!this->process_pending_())
      this->disable_loop();
  }

  void play(const Ts &...x) override { /* see play_complex */ }

 protected:
  /// Longest time one loop() pass spends reading a response body
  static constexpr uint32_t READ_BUDGET_MS = 10;
  /// Requests of one action that run at once; each holds a client, and a TLS client holds about 40 kB
  static constexpr uint8_t MAX_ACTIVE = 1;
  /// Requests that wait for a free slot; more are dropped with on_error
  static constexpr uint8_t MAX_QUEUED = 4;

  struct PendingRequest {
    explicit PendingRequest(const Ts &...x) : args(x...) {}
    PendingRequest(const PendingRequest &) = delete;
    PendingRequest &operator=(const PendingRequest &) = delete;
    ~PendingRequest() { this->free_buffer(); }
    void free_buffer() {
      if (this->buf != nullptr)
        RAMAllocator<uint8_t>().deallocate(this->buf, this->buf_size);
      this->buf = nullptr;
    }

    // nullptr until the request has started
    std::shared_ptr<HttpContainer> container;
    uint8_t *buf{nullptr};
    size_t buf_size{0};
    size_t read_index{0};
    uint32_t last_data_time{0};
    uint32_t timeout{0};
    bool capture{false};
    bool reading{false};
    bool failed{false};
    std::tuple<stored_trigger_arg_t<Ts>...> args;
  };

  void stop() override {
    this->end_all_(this->pending_);
    this->active_ = 0;
    this->queued_ = 0;
    // Tell any process_pending_() further down the stack that the requests it holds were cancelled
    this->stop_generation_++;
    this->disable_loop();
  }

  void end_all_(std::list<PendingRequest> &requests) {
    for (auto &request : requests) {
      if (request.container != nullptr)
        request.container->end();
    }
    requests.clear();
  }

  /// Build the request from the stored trigger arguments and start it. Returns false if it could not start.
  bool start_(PendingRequest &request) {
    std::apply([this, &request](auto &...x) { this->start_request_(request, x...); }, request.args);
    return request.container != nullptr;
  }

  void start_request_(PendingRequest &request, const Ts &...x) {
    std::string body;
    if (this->body_.has_value()) {
      body = this->body_.value(x...);
    }
    if (!this->json_.empty()) {
      body = json::build_json([this, x...](JsonObject root) mutable { this->encode_json_(x..., root); });
    }
    if (this->json_func_ != nullptr) {
      body = json::build_json([this, x...](JsonObject root) mutable { this->json_func_(x..., root); });
    }
    std::vector<Header> request_headers;
    request_headers.reserve(this->request_headers_.size());
    for (const auto &header : this->request_headers_) {
      request_headers.push_back({header.name, header.value(x...)});
    }

    request.timeout = this->timeout_.has_value() ? this->timeout_.value(x...) : this->parent_->get_timeout();
#ifdef USE_HTTP_REQUEST_RESPONSE
    request.capture = this->capture_response_.value(x...);
#endif
    request.container = this->parent_->begin(this->url_.value(x...).c_str(), this->method_.value(x...), std::move(body),
                                             request_headers, this->lower_case_collect_headers_, request.timeout);
  }

  /// Returns true if requests are still in progress
  bool process_pending_() {
    // A finished request runs the rest of the action chain, which can start this action again
    // or stop it, so work on a local list rather than the member.
    std::list<PendingRequest> queue;
    queue.swap(this->pending_);
    std::list<PendingRequest> waiting;
    while (!queue.empty()) {
      auto it = queue.begin();
      const uint8_t generation = this->stop_generation_;
      // advance_() can start a waiting request, whose lambdas can stop this action
      const bool finished = this->advance_(*it);
      if (generation == this->stop_generation_) {
        if (!finished) {
          waiting.splice(waiting.end(), queue, it);
          continue;
        }
        std::list<PendingRequest> completed;
        completed.splice(completed.begin(), queue, it);
        this->finish_(completed.front());
      }
      if (generation != this->stop_generation_) {
        this->end_all_(waiting);
        this->end_all_(queue);
        break;
      }
    }
    // Keep the older requests ahead of any started from inside the chain
    this->pending_.splice(this->pending_.begin(), waiting);
    return !this->pending_.empty();
  }

  /// Returns true once the request has finished and its triggers can run
  bool advance_(PendingRequest &request) {
    if (request.container == nullptr) {
      if (this->active_ >= MAX_ACTIVE)
        return false;
      this->queued_--;
      this->active_++;
      if (!this->start_(request)) {
        request.failed = true;
        return true;
      }
    }
    if (!request.reading) {
      auto progress = request.container->poll();
      if (progress == HttpRequestProgress::HTTP_REQUEST_PROGRESS_PENDING)
        return false;
      if (progress == HttpRequestProgress::HTTP_REQUEST_PROGRESS_FAILED) {
        request.failed = true;
        return true;
      }
      if (!request.capture)
        return true;
      request.buf = RAMAllocator<uint8_t>().allocate(this->max_response_buffer_size_);
      if (request.buf == nullptr)
        return true;
      request.buf_size = this->max_response_buffer_size_;
      request.reading = true;
      request.last_data_time = App.get_loop_component_start_time();
    }
    // NOTE: HttpContainer::read() has non-BSD socket semantics - see top of this file
    // millis(), not the loop time: the budget is shorter than one pass of the loop
    const uint32_t start = millis();
    while (request.read_index < request.buf_size) {
      int read_or_error =
          request.container->read_available(request.buf + request.read_index, request.buf_size - request.read_index);
      const uint32_t now = millis();
      if (read_or_error < 0)
        return true;
      if (read_or_error == 0) {
        return request.container->is_read_complete() || now - request.last_data_time >= request.timeout;
      }
      request.read_index += read_or_error;
      request.last_data_time = now;
      if (now - start >= READ_BUDGET_MS)
        return false;
    }
    return true;
  }

  /// Fire on_error for a request that was never started, then continue the chain
  void report_error_(const Ts &...x) {
    const uint8_t generation = this->stop_generation_;
    this->error_trigger_.trigger(x...);
    // A stop from the trigger ended this run; a run started after it continues the chain instead
    if (generation == this->stop_generation_)
      this->play_next_(x...);
  }

  void finish_(PendingRequest &request) {
    // Free the slot first, so that the chain can start this action again
    this->active_--;
    const uint8_t generation = this->stop_generation_;
    auto &container = request.container;
    if (request.failed) {
      std::apply([this](auto &...args) { this->error_trigger_.trigger(args...); }, request.args);
    }
#ifdef USE_HTTP_REQUEST_RESPONSE
    else if (request.capture) {
      std::string response_body;
      if (request.buf != nullptr)
        response_body.assign(reinterpret_cast<char *>(request.buf), request.read_index);
      request.free_buffer();
      std::apply(
          [this, &container, &response_body](auto &...args) {
            this->success_trigger_with_response_.trigger(container, response_body, args...);
          },
          request.args);
    }
#endif
    else {
      std::apply([this, &container](auto &...args) { this->success_trigger_.trigger(container, args...); },
                 request.args);
    }
    if (container != nullptr)
      container->end();
    // A stop from the triggers ended this run; a run started after it continues the chain instead
    if (generation == this->stop_generation_)
      std::apply([this](auto &...args) { this->play_next_(args...); }, request.args);
  }

  void encode_json_(Ts... x, JsonObject root) {
    for (const auto &item : this->json_) {
      auto val = item.second;
      root[item.first] = val.value(x...);
    }
  }
  HttpRequestComponent *parent_;
  ConstVector<RequestHeader<Ts...>> request_headers_{};
  std::vector<std::string> lower_case_collect_headers_{"content-type", "content-length"};
  FixedVector<std::pair<const char *, TemplatableValue<std::string, Ts...>>> json_{};
  std::function<void(Ts..., JsonObject)> json_func_{nullptr};
#ifdef USE_HTTP_REQUEST_RESPONSE
  Trigger<std::shared_ptr<HttpContainer>, std::string &, Ts...> success_trigger_with_response_;
#endif
  Trigger<std::shared_ptr<HttpContainer>, Ts...> success_trigger_;
  Trigger<Ts...> error_trigger_;
  std::list<PendingRequest> pending_{};

  size_t max_response_buffer_size_{SIZE_MAX};
  // Started and unfinished requests, and requests waiting to start
  uint8_t active_{0};
  uint8_t queued_{0};
  // Bumped by stop() so process_pending_() can detect a stop from inside the action chain
  uint8_t stop_generation_{0};
};

}  // namespace esphome::http_request
