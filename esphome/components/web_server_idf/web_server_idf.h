#pragma once
#ifdef USE_ESP32

#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/string_ref.h"
#include <esp_http_server.h>

#include <atomic>
#include <functional>
#include <list>
#include <map>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "esphome/components/web_server_base/stream_head.h"

struct iovec;  // NOLINT(readability-identifier-naming) - forward decl of lwip's gather list entry

namespace esphome::web_server_idf {

using web_server_base::HttpHeader;
using web_server_base::StallClock;

class AsyncWebParameter {
 public:
  AsyncWebParameter(std::string name, std::string value) : name_(std::move(name)), value_(std::move(value)) {}
  const std::string &name() const { return this->name_; }
  const std::string &value() const { return this->value_; }

 protected:
  std::string name_;
  std::string value_;
};

class AsyncWebServerRequest;

class AsyncWebServerResponse {
  friend class AsyncWebServerRequest;

 public:
  AsyncWebServerResponse(const AsyncWebServerRequest *req) : req_(req) {}
  virtual ~AsyncWebServerResponse() {}

  // NOLINTNEXTLINE(readability-identifier-naming)
  void addHeader(const char *name, const char *value);

  virtual const char *get_content_data() const = 0;
  virtual size_t get_content_size() const = 0;

 protected:
  const AsyncWebServerRequest *req_;
  // Bit i set: this response set default header i itself, so it is not sent at send time
  uint8_t replaced_defaults_{0};
};

class AsyncWebServerResponseEmpty : public AsyncWebServerResponse {
 public:
  AsyncWebServerResponseEmpty(const AsyncWebServerRequest *req) : AsyncWebServerResponse(req) {}

  const char *get_content_data() const override { return nullptr; };
  size_t get_content_size() const override { return 0; };
};

class AsyncWebServerResponseContent : public AsyncWebServerResponse {
 public:
  AsyncWebServerResponseContent(const AsyncWebServerRequest *req, std::string content)
      : AsyncWebServerResponse(req), content_(std::move(content)) {}

  const char *get_content_data() const override { return this->content_.c_str(); };
  size_t get_content_size() const override { return this->content_.size(); };

 protected:
  std::string content_;
};

class AsyncResponseStream : public AsyncWebServerResponse {
 public:
  AsyncResponseStream(const AsyncWebServerRequest *req) : AsyncWebServerResponse(req) {}

  const char *get_content_data() const override { return this->content_.c_str(); };
  size_t get_content_size() const override { return this->content_.size(); };

  void print(const char *str) { this->content_.append(str); }
  void print(const std::string &str) { this->content_.append(str); }
  void print(float value);
  void printf(const char *fmt, ...) __attribute__((format(printf, 2, 3)));
  void write(uint8_t c) { this->content_.push_back(static_cast<char>(c)); }

 protected:
  std::string content_;
};

class AsyncWebServerResponseProgmem : public AsyncWebServerResponse {
 public:
  AsyncWebServerResponseProgmem(const AsyncWebServerRequest *req, const uint8_t *data, const size_t size)
      : AsyncWebServerResponse(req), data_(data), size_(size) {}

  const char *get_content_data() const override { return reinterpret_cast<const char *>(this->data_); };
  size_t get_content_size() const override { return this->size_; };

 protected:
  const uint8_t *data_;
  size_t size_;
};

class AsyncWebServerRequest {
  friend class AsyncWebServer;

 public:
  ~AsyncWebServerRequest();

  http_method method() const { return static_cast<http_method>(this->req_->method); }
  static constexpr size_t URL_BUF_SIZE = CONFIG_HTTPD_MAX_URI_LEN + 1;  ///< Buffer size for url_to()
  /// Write URL (without query string) to buffer, returns StringRef pointing to buffer.
  /// URL is decoded (e.g., %20 -> space).
  StringRef url_to(std::span<char, URL_BUF_SIZE> buffer) const;
  // NOLINTNEXTLINE(readability-identifier-naming)
  size_t contentLength() const { return this->req_->content_len; }

#ifdef USE_WEBSERVER_AUTH
  bool authenticate(const char *username, const char *password) const;
  // NOLINTNEXTLINE(readability-identifier-naming)
  void requestAuthentication() const;
#endif

  void redirect(const char *url);
  void redirect(const std::string &url) { this->redirect(url.c_str()); }

  void send(AsyncWebServerResponse *response);
  inline void ESPHOME_ALWAYS_INLINE send(int code, const char *content_type = nullptr, const char *content = nullptr) {
    this->init_response_(nullptr, code, content_type);
    if (content) {
      httpd_resp_send(*this, content, HTTPD_RESP_USE_STRLEN);
    } else {
      httpd_resp_send(*this, nullptr, 0);
    }
  }
  // NOLINTNEXTLINE(readability-identifier-naming)
  AsyncWebServerResponse *beginResponse(int code, const char *content_type) {
    auto *res = new AsyncWebServerResponseEmpty(this);  // NOLINT(cppcoreguidelines-owning-memory)
    this->init_response_(res, code, content_type);
    return res;
  }
  // NOLINTNEXTLINE(readability-identifier-naming)
  AsyncWebServerResponse *beginResponse(int code, const char *content_type, const std::string &content) {
    auto *res = new AsyncWebServerResponseContent(this, content);  // NOLINT(cppcoreguidelines-owning-memory)
    this->init_response_(res, code, content_type);
    return res;
  }
  // NOLINTNEXTLINE(readability-identifier-naming)
  AsyncWebServerResponse *beginResponse(int code, const char *content_type, const uint8_t *data,
                                        const size_t data_size) {
    auto *res = new AsyncWebServerResponseProgmem(this, data, data_size);  // NOLINT(cppcoreguidelines-owning-memory)
    this->init_response_(res, code, content_type);
    return res;
  }
  // NOLINTNEXTLINE(readability-identifier-naming)
  AsyncResponseStream *beginResponseStream(const char *content_type) {
    auto *res = new AsyncResponseStream(this);  // NOLINT(cppcoreguidelines-owning-memory)
    this->init_response_(res, 200, content_type);
    return res;
  }

  // NOLINTNEXTLINE(readability-identifier-naming)
  bool hasParam(const char *name) { return this->getParam(name) != nullptr; }
  // NOLINTNEXTLINE(readability-identifier-naming)
  bool hasParam(const std::string &name) { return this->getParam(name.c_str()) != nullptr; }
  // NOLINTNEXTLINE(readability-identifier-naming)
  AsyncWebParameter *getParam(const char *name);
  // NOLINTNEXTLINE(readability-identifier-naming)
  AsyncWebParameter *getParam(const std::string &name) { return this->getParam(name.c_str()); }

  // NOLINTNEXTLINE(readability-identifier-naming)
  bool hasArg(const char *name);
  std::string arg(const char *name);
  std::string arg(const std::string &name) { return this->arg(name.c_str()); }

#ifdef USE_WEBSERVER_OTA
  /// Called when the request ends, including early returns.
  // NOLINTNEXTLINE(readability-identifier-naming)
  void onDisconnect(std::function<void()> fn) { this->on_disconnect_ = std::move(fn); }
#endif

  /// sess_ctx is reserved for AsyncStreamResponse: the server's close_fn treats any session context as one.
  operator httpd_req_t *() const { return this->req_; }
  optional<std::string> get_header(const char *name) const;
  /// Copy a header value into buf without heap allocation.
  /// Returns its length, 0 when absent, or SIZE_MAX when it does not fit in buf.
  size_t get_header_to(const char *name, std::span<char> buf) const;
  // NOLINTNEXTLINE(readability-identifier-naming)
  bool hasHeader(const char *name) const;

 protected:
  httpd_req_t *req_;
  AsyncWebServerResponse *rsp_{};
  // Use vector instead of map/unordered_map: most requests have 0-3 params, so linear search
  // is faster than tree/hash overhead. AsyncWebParameter stores both name and value to avoid
  // duplicate storage. Only successful lookups are cached to prevent cache pollution when
  // handlers check for optional parameters that don't exist.
  optional<std::string> find_query_value_(const char *name) const;
  std::vector<AsyncWebParameter *> params_;
  std::string post_query_;
#ifdef USE_WEBSERVER_OTA
  std::function<void()> on_disconnect_;
#endif
  AsyncWebServerRequest(httpd_req_t *req) : req_(req) {}
  AsyncWebServerRequest(httpd_req_t *req, std::string post_query) : req_(req), post_query_(std::move(post_query)) {}
  void init_response_(AsyncWebServerResponse *rsp, int code, const char *content_type);
};

class AsyncWebHandler;

class AsyncWebServer {
 public:
  AsyncWebServer(uint16_t port) : port_(port) {};
  ~AsyncWebServer() { this->end(); }

  // NOLINTNEXTLINE(readability-identifier-naming)
  void onNotFound(std::function<void(AsyncWebServerRequest *request)> &&fn) { on_not_found_ = std::move(fn); }

  void begin();
  void end();

  // NOLINTNEXTLINE(readability-identifier-naming)
  AsyncWebHandler &addHandler(AsyncWebHandler *handler) {
    this->handlers_.push_back(handler);
    return *handler;
  }

  httpd_handle_t get_server() { return this->server_; }

 protected:
  uint16_t port_{};
  httpd_handle_t server_{};
  static esp_err_t request_handler(httpd_req_t *r);
  static esp_err_t request_post_handler(httpd_req_t *r);
  esp_err_t request_handler_(AsyncWebServerRequest *request) const;
  static void safe_close_with_shutdown(httpd_handle_t hd, int sockfd);
  esp_err_t handle_raw_body_(httpd_req_t *r, const char *content_type);
#ifdef USE_WEBSERVER_OTA
  esp_err_t handle_multipart_upload_(httpd_req_t *r, const char *content_type);
#endif
  std::vector<AsyncWebHandler *> handlers_;
  std::function<void(AsyncWebServerRequest *request)> on_not_found_{};
};

class AsyncWebHandler {
 public:
  virtual ~AsyncWebHandler() {}
  // NOLINTNEXTLINE(readability-identifier-naming)
  virtual bool canHandle(AsyncWebServerRequest *request) const { return false; }
  // NOLINTNEXTLINE(readability-identifier-naming)
  virtual void handleRequest(AsyncWebServerRequest *request) {}
  // NOLINTNEXTLINE(readability-identifier-naming)
  virtual void handleUpload(AsyncWebServerRequest *request, const std::string &filename, size_t index, uint8_t *data,
                            size_t len, bool final) {}
  // NOLINTNEXTLINE(readability-identifier-naming)
  virtual void handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {}
  // NOLINTNEXTLINE(readability-identifier-naming)
  virtual bool isRequestHandlerTrivial() const { return true; }
};

/// A response that keeps its connection open and is written from the main loop, such as an event stream or
/// an audio stream. Use it directly or derive from it. Call loop() and delete the object only through the type
/// it was created as: subclasses hide loop() instead of overriding it, and the destructor is not virtual.
///
/// Lifecycle:
///  1. Httpd task, inside handleRequest(): create it and call begin(), then hand the pointer to the main loop.
///  2. Main loop: once is_attached() is true, write() may go through. Call loop() on every pass.
///  3. Main loop: delete it once safe_to_delete() is true. httpd owns the socket throughout.
class AsyncStreamResponse {
 public:
  /// Httpd task, once. Sends the status line (for example HTTPD_200), Content-Type, headers, the default headers
  /// and body (a C string or null, sent as is), then makes this object the session context. A chunked response
  /// passes {"Transfer-Encoding", "chunked"} as its first header and frames each chunk itself.
  void begin(AsyncWebServerRequest *request, const char *status, const char *content_type,
             std::span<const HttpHeader> headers, const char *body = nullptr);

  /// Main loop. Non-blocking gather write. Returns the bytes accepted, which may be fewer than offered: the caller
  /// keeps the rest and continues from there. 0 means the socket is full or not ours yet; -1 means the stream failed
  /// and is closing. Refused writes for SEND_STALL_TIMEOUT_MS in a row also close it.
  ssize_t write(struct iovec *iov, int iovcnt);
  ssize_t write(const void *data, size_t len);
  /// Main loop. Shuts the connection down through httpd; safe to call more than once.
  void close();
  /// Main loop, every pass. Keeps the session fresh in httpd's LRU and retries a pending close.
  void loop();
  /// Httpd task, from the server's close_fn: no write() reaches the socket after this returns.
  static void socket_closing(httpd_handle_t hd, int fd);

  /// httpd holds this response as the session context; begin() has sent the whole head by then.
  bool is_attached() const { return this->owns_session_(this->fd_.load()); }
  /// The client is still connected; httpd has not freed the session.
  bool is_connected() const { return this->fd_.load() != 0; }
  bool is_closing() const { return this->close_requested_; }
  /// The socket fd, for log lines only
  int log_id() const { return this->fd_.load(); }
  /// Sends are making no progress; after write() returns -1 this tells a send timeout from a send error.
  bool is_stalled() const { return this->stall_.is_running(); }
  /// The session is gone and no queued httpd work still references this object.
  bool safe_to_delete() const {
    return this->fd_.load() == 0 && !this->close_work_queued_.load(std::memory_order_acquire) &&
           !this->lru_work_queued_.load(std::memory_order_acquire);
  }

  static constexpr uint32_t SEND_STALL_TIMEOUT_MS = 20000;

 protected:
  void process_close_();
  static void close_session_work(void *arg);
  void refresh_lru_();
  static void refresh_lru_work(void *arg);
  // httpd closes the socket before it clears the session, so this can still match a closed fd; write() also checks
  // socket_closed_
  bool owns_session_(int fd) const { return fd != 0 && httpd_sess_get_ctx(this->hd_, fd) == this; }
  static void destroy(void *p);

  httpd_handle_t hd_{};
  std::atomic<int> fd_{};
  // Runs while sends make no progress; a running clock keeps its start time
  StallClock stall_;
  uint32_t next_close_attempt_ms_{0};
  uint32_t last_lru_refresh_ms_{0};
  // Set on the main loop before queueing close work, cleared by the HTTPD-task callback when done.
  std::atomic<bool> close_work_queued_{false};
  std::atomic<bool> lru_work_queued_{false};
  // Guarded by the stream send mutex
  bool socket_closed_{false};
  // Main-loop only; the HTTPD task never reads or writes these flags.
  bool close_requested_{false};
  bool close_retry_warning_logged_{false};

  static constexpr uint32_t CLOSE_RETRY_INTERVAL_MS = 250;
  static constexpr uint32_t CLOSE_CONFIRM_INTERVAL_MS = 1000;
  static constexpr uint32_t LRU_REFRESH_INTERVAL_MS = 1000;
};


class DefaultHeaders {
  friend class AsyncWebServerRequest;
  friend class AsyncStreamResponse;
  friend class AsyncWebServerResponse;
  static_assert(WEB_SERVER_DEFAULT_HEADERS_COUNT <= 8, "AsyncWebServerResponse::replaced_defaults_ holds 8 bits");

 public:
  // NOLINTNEXTLINE(readability-identifier-naming)
  void addHeader(const char *name, const char *value) { this->headers_.push_back({name, value}); }

  // NOLINTNEXTLINE(readability-identifier-naming)
  static DefaultHeaders &Instance();

 protected:
  // Stack-allocated, no reallocation machinery. Count defined in web_server_base where headers are added.
  StaticVector<HttpHeader, WEB_SERVER_DEFAULT_HEADERS_COUNT> headers_;

  /// Set every default header on the response except those whose bit is set in skip_mask.
  void apply_(httpd_req_t *req, uint8_t skip_mask = 0) const;
};

}  // namespace esphome::web_server_idf

using namespace esphome::web_server_idf;  // NOLINT(google-global-names-in-headers)

#endif  // !defined(USE_ESP32)
