#pragma once

#include <list>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "esphome/components/json/json_util.h"
#include "esphome/core/application.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::http_request {

struct Header {
  std::string name;
  std::string value;
};

// Some common HTTP status codes
enum HttpStatus {
  HTTP_STATUS_OK = 200,
  HTTP_STATUS_NO_CONTENT = 204,
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
  DATA,     ///< Data was read, process it
  RETRY,    ///< No data yet, already delayed, caller should continue loop
  ERROR,    ///< Read error, caller should exit loop
  TIMEOUT,  ///< Timeout waiting for data, caller should exit loop
};

/// Process a read result with timeout tracking and delay handling
/// @param bytes_read_or_error Return value from read() - positive for bytes read, negative for error
/// @param last_data_time Time of last successful read, updated when data received
/// @param timeout_ms Maximum time to wait for data
/// @return DATA if data received, RETRY if should continue loop, ERROR/TIMEOUT if should exit
inline HttpReadLoopResult http_read_loop_result(int bytes_read_or_error, uint32_t &last_data_time,
                                                uint32_t timeout_ms) {
  if (bytes_read_or_error > 0) {
    last_data_time = millis();
    return HttpReadLoopResult::DATA;
  }
  if (bytes_read_or_error < 0) {
    return HttpReadLoopResult::ERROR;
  }
  // bytes_read_or_error == 0: no data available yet
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
  size_t content_length;
  int status_code;
  uint32_t duration_ms;

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
  virtual void end() = 0;

  void set_secure(bool secure) { this->secure_ = secure; }

  size_t get_bytes_read() const { return this->bytes_read_; }

  /**
   * @brief Get response headers.
   *
   * @return The key is the lower case response header name, the value is the header value.
   */
  std::map<std::string, std::list<std::string>> get_response_headers() { return this->response_headers_; }

  std::string get_response_header(const std::string &header_name);

 protected:
  size_t bytes_read_{0};
  bool secure_{false};
  std::map<std::string, std::list<std::string>> response_headers_{};
};

/// Read data from HTTP container into buffer with timeout handling
/// Handles feed_wdt, yield, and timeout checking internally
/// @param container The HTTP container to read from
/// @param buffer Buffer to read into
/// @param total_size Total bytes to read
/// @param chunk_size Maximum bytes per read call
/// @param timeout_ms Read timeout in milliseconds
/// @return HttpReadResult with status and error_code on failure
inline HttpReadResult http_read_fully(HttpContainer *container, uint8_t *buffer, size_t total_size, size_t chunk_size,
                                      uint32_t timeout_ms) {
  size_t read_index = 0;
  uint32_t last_data_time = millis();

  while (read_index < total_size) {
    int read_bytes_or_error = container->read(buffer + read_index, std::min(chunk_size, total_size - read_index));

    App.feed_wdt();
    yield();

    auto result = http_read_loop_result(read_bytes_or_error, last_data_time, timeout_ms);
    if (result == HttpReadLoopResult::RETRY)
      continue;
    if (result == HttpReadLoopResult::ERROR)
      return {HttpReadStatus::ERROR, read_bytes_or_error};
    if (result == HttpReadLoopResult::TIMEOUT)
      return {HttpReadStatus::TIMEOUT, 0};

    read_index += read_bytes_or_error;
  }
  return {HttpReadStatus::OK, 0};
}

class HttpRequestResponseTrigger : public Trigger<std::shared_ptr<HttpContainer>, std::string &> {
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
  void set_follow_redirects(bool follow_redirects) { this->follow_redirects_ = follow_redirects; }
  void set_redirect_limit(uint16_t limit) { this->redirect_limit_ = limit; }

  std::shared_ptr<HttpContainer> get(const std::string &url) { return this->start(url, "GET", "", {}); }
  std::shared_ptr<HttpContainer> get(const std::string &url, const std::list<Header> &request_headers) {
    return this->start(url, "GET", "", request_headers);
  }
  std::shared_ptr<HttpContainer> get(const std::string &url, const std::list<Header> &request_headers,
                                     const std::set<std::string> &collect_headers) {
    return this->start(url, "GET", "", request_headers, collect_headers);
  }
  std::shared_ptr<HttpContainer> post(const std::string &url, const std::string &body) {
    return this->start(url, "POST", body, {});
  }
  std::shared_ptr<HttpContainer> post(const std::string &url, const std::string &body,
                                      const std::list<Header> &request_headers) {
    return this->start(url, "POST", body, request_headers);
  }
  std::shared_ptr<HttpContainer> post(const std::string &url, const std::string &body,
                                      const std::list<Header> &request_headers,
                                      const std::set<std::string> &collect_headers) {
    return this->start(url, "POST", body, request_headers, collect_headers);
  }

  std::shared_ptr<HttpContainer> start(const std::string &url, const std::string &method, const std::string &body,
                                       const std::list<Header> &request_headers) {
    return this->start(url, method, body, request_headers, {});
  }

  std::shared_ptr<HttpContainer> start(const std::string &url, const std::string &method, const std::string &body,
                                       const std::list<Header> &request_headers,
                                       const std::set<std::string> &collect_headers) {
    std::set<std::string> lower_case_collect_headers;
    for (const std::string &collect_header : collect_headers) {
      lower_case_collect_headers.insert(str_lower_case(collect_header));
    }
    return this->perform(url, method, body, request_headers, lower_case_collect_headers);
  }

 protected:
  virtual std::shared_ptr<HttpContainer> perform(const std::string &url, const std::string &method,
                                                 const std::string &body, const std::list<Header> &request_headers,
                                                 const std::set<std::string> &collect_headers) = 0;
  const char *useragent_{nullptr};
  bool follow_redirects_{};
  uint16_t redirect_limit_{};
  uint32_t timeout_{4500};
  uint32_t watchdog_timeout_{0};
};

template<typename... Ts> class HttpRequestSendAction : public Action<Ts...> {
 public:
  HttpRequestSendAction(HttpRequestComponent *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, url)
  TEMPLATABLE_VALUE(const char *, method)
  TEMPLATABLE_VALUE(std::string, body)
#ifdef USE_HTTP_REQUEST_RESPONSE
  TEMPLATABLE_VALUE(bool, capture_response)
#endif

  void add_request_header(const char *key, TemplatableValue<const char *, Ts...> value) {
    this->request_headers_.insert({key, value});
  }

  void add_collect_header(const char *value) { this->collect_headers_.insert(value); }

  void add_json(const char *key, TemplatableValue<std::string, Ts...> value) { this->json_.insert({key, value}); }

  void set_json(std::function<void(Ts..., JsonObject)> json_func) { this->json_func_ = json_func; }

#ifdef USE_HTTP_REQUEST_RESPONSE
  Trigger<std::shared_ptr<HttpContainer>, std::string &, Ts...> *get_success_trigger_with_response() const {
    return this->success_trigger_with_response_;
  }
#endif
  Trigger<std::shared_ptr<HttpContainer>, Ts...> *get_success_trigger() const { return this->success_trigger_; }

  Trigger<Ts...> *get_error_trigger() const { return this->error_trigger_; }

  void set_max_response_buffer_size(size_t max_response_buffer_size) {
    this->max_response_buffer_size_ = max_response_buffer_size;
  }

  void play(const Ts &...x) override {
    std::string body;
    if (this->body_.has_value()) {
      body = this->body_.value(x...);
    }
    if (!this->json_.empty()) {
      auto f = std::bind(&HttpRequestSendAction<Ts...>::encode_json_, this, x..., std::placeholders::_1);
      body = json::build_json(f);
    }
    if (this->json_func_ != nullptr) {
      auto f = std::bind(&HttpRequestSendAction<Ts...>::encode_json_func_, this, x..., std::placeholders::_1);
      body = json::build_json(f);
    }
    std::list<Header> request_headers;
    for (const auto &item : this->request_headers_) {
      auto val = item.second;
      Header header;
      header.name = item.first;
      header.value = val.value(x...);
      request_headers.push_back(header);
    }

    auto container = this->parent_->start(this->url_.value(x...), this->method_.value(x...), body, request_headers,
                                          this->collect_headers_);

    auto captured_args = std::make_tuple(x...);

    if (container == nullptr) {
      std::apply([this](Ts... captured_args_inner) { this->error_trigger_->trigger(captured_args_inner...); },
                 captured_args);
      return;
    }

    size_t max_length = this->max_response_buffer_size_;
#ifdef USE_HTTP_REQUEST_RESPONSE
    if (this->capture_response_.value(x...)) {
      std::string response_body;
      RAMAllocator<uint8_t> allocator;
      uint8_t *buf = allocator.allocate(max_length);
      if (buf != nullptr) {
        // NOTE: HttpContainer::read() has non-BSD socket semantics - see top of this file
        // Use http_read_loop_result() helper instead of checking return values directly
        size_t read_index = 0;
        uint32_t last_data_time = millis();
        const uint32_t read_timeout = this->parent_->get_timeout();
        while (container->get_bytes_read() < max_length) {
          int read_or_error = container->read(buf + read_index, std::min<size_t>(max_length - read_index, 512));
          App.feed_wdt();
          yield();
          auto result = http_read_loop_result(read_or_error, last_data_time, read_timeout);
          if (result == HttpReadLoopResult::RETRY)
            continue;
          if (result != HttpReadLoopResult::DATA)
            break;  // ERROR or TIMEOUT
          read_index += read_or_error;
        }
        response_body.reserve(read_index);
        response_body.assign((char *) buf, read_index);
        allocator.deallocate(buf, max_length);
      }
      std::apply(
          [this, &container, &response_body](Ts... captured_args_inner) {
            this->success_trigger_with_response_->trigger(container, response_body, captured_args_inner...);
          },
          captured_args);
    } else
#endif
    {
      std::apply([this, &container](
                     Ts... captured_args_inner) { this->success_trigger_->trigger(container, captured_args_inner...); },
                 captured_args);
    }
    container->end();
  }

 protected:
  void encode_json_(Ts... x, JsonObject root) {
    for (const auto &item : this->json_) {
      auto val = item.second;
      root[item.first] = val.value(x...);
    }
  }
  void encode_json_func_(Ts... x, JsonObject root) { this->json_func_(x..., root); }
  HttpRequestComponent *parent_;
  std::map<const char *, TemplatableValue<const char *, Ts...>> request_headers_{};
  std::set<std::string> collect_headers_{"content-type", "content-length"};
  std::map<const char *, TemplatableValue<std::string, Ts...>> json_{};
  std::function<void(Ts..., JsonObject)> json_func_{nullptr};
#ifdef USE_HTTP_REQUEST_RESPONSE
  Trigger<std::shared_ptr<HttpContainer>, std::string &, Ts...> *success_trigger_with_response_ =
      new Trigger<std::shared_ptr<HttpContainer>, std::string &, Ts...>();
#endif
  Trigger<std::shared_ptr<HttpContainer>, Ts...> *success_trigger_ =
      new Trigger<std::shared_ptr<HttpContainer>, Ts...>();
  Trigger<Ts...> *error_trigger_ = new Trigger<Ts...>();

  size_t max_response_buffer_size_{SIZE_MAX};
};

}  // namespace esphome::http_request
