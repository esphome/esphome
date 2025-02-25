#pragma once
#ifdef USE_ESP_IDF

#include <esp_http_server.h>

#include <functional>
#include <list>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace esphome {
namespace web_server {
class WebServer;
class ListEntitiesIterator;
};  // namespace web_server
namespace web_server_idf {

#define F(string_literal) (string_literal)
#define PGM_P const char *
#define strncpy_P strncpy

using String = std::string;

class AsyncWebParameter {
 public:
  AsyncWebParameter(std::string value) : value_(std::move(value)) {}
  const std::string &value() const { return this->value_; }

 protected:
  std::string value_;
};

class AsyncWebServerRequest;

class AsyncWebServerResponse {
 public:
  AsyncWebServerResponse(const AsyncWebServerRequest *req) : req_(req) {}
  virtual ~AsyncWebServerResponse() {}

  // NOLINTNEXTLINE(readability-identifier-naming)
  void addHeader(const char *name, const char *value);

  virtual const char *get_content_data() const = 0;
  virtual size_t get_content_size() const = 0;

 protected:
  const AsyncWebServerRequest *req_;
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
  std::string url() const;
  std::string host() const;
  // NOLINTNEXTLINE(readability-identifier-naming)
  size_t contentLength() const { return this->req_->content_len; }

  bool authenticate(const char *username, const char *password) const;
  // NOLINTNEXTLINE(readability-identifier-naming)
  void requestAuthentication(const char *realm = nullptr) const;

  void redirect(const std::string &url);

  void send(AsyncWebServerResponse *response);
  void send(int code, const char *content_type = nullptr, const char *content = nullptr);
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
  AsyncWebServerResponse *beginResponse_P(int code, const char *content_type, const uint8_t *data,
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
  bool hasParam(const std::string &name) { return this->getParam(name) != nullptr; }
  // NOLINTNEXTLINE(readability-identifier-naming)
  AsyncWebParameter *getParam(const std::string &name);

  // NOLINTNEXTLINE(readability-identifier-naming)
  bool hasArg(const char *name) { return this->hasParam(name); }
  std::string arg(const std::string &name) {
    auto *param = this->getParam(name);
    if (param) {
      return param->value();
    }
    return {};
  }

  operator httpd_req_t *() const { return this->req_; }
  optional<std::string> get_header(const char *name) const;
  // NOLINTNEXTLINE(readability-identifier-naming)
  bool hasHeader(const char *name) const;

 protected:
  httpd_req_t *req_;
  AsyncWebServerResponse *rsp_{};
  std::map<std::string, AsyncWebParameter *> params_;
  std::string post_query_;
  AsyncWebServerRequest(httpd_req_t *req) : req_(req) {}
  AsyncWebServerRequest(httpd_req_t *req, std::string post_query) : req_(req), post_query_(std::move(post_query)) {}
  void init_response_(AsyncWebServerResponse *rsp, int code, const char *content_type);
};

class AsyncWebHandler;

class AsyncWebServer {
 public:
  AsyncWebServer(uint16_t port) : port_(port){};
  ~AsyncWebServer() { this->end(); }

  // NOLINTNEXTLINE(readability-identifier-naming)
  void onNotFound(std::function<void(AsyncWebServerRequest *request)> fn) { on_not_found_ = std::move(fn); }

  void begin();
  void end();

  // NOLINTNEXTLINE(readability-identifier-naming)
  AsyncWebHandler &addHandler(AsyncWebHandler *handler) {
    this->handlers_.push_back(handler);
    return *handler;
  }

 protected:
  uint16_t port_{};
  httpd_handle_t server_{};
  static esp_err_t request_handler(httpd_req_t *r);
  static esp_err_t request_post_handler(httpd_req_t *r);
  esp_err_t request_handler_(AsyncWebServerRequest *request) const;
  std::vector<AsyncWebHandler *> handlers_;
  std::function<void(AsyncWebServerRequest *request)> on_not_found_{};
};

class AsyncWebHandler {
 public:
  virtual ~AsyncWebHandler() {}
  // NOLINTNEXTLINE(readability-identifier-naming)
  virtual bool canHandle(AsyncWebServerRequest *request) { return false; }
  // NOLINTNEXTLINE(readability-identifier-naming)
  virtual void handleRequest(AsyncWebServerRequest *request) {}
  // NOLINTNEXTLINE(readability-identifier-naming)
  virtual void handleUpload(AsyncWebServerRequest *request, const std::string &filename, size_t index, uint8_t *data,
                            size_t len, bool final) {}
  // NOLINTNEXTLINE(readability-identifier-naming)
  virtual void handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {}
  // NOLINTNEXTLINE(readability-identifier-naming)
  virtual bool isRequestHandlerTrivial() { return true; }
};

class AsyncEventSource;
class AsyncEventSourceResponse;

using message_generator_t = std::string(esphome::web_server::WebServer *, void *);

/*
  This class holds a pointer to the source component that wants to publish a state event, and a pointer to a function
  that will lazily generate that event.  The two pointers allow dedup in the deferred queue if multiple publishes for
  the same component are backed up, and take up only 8 bytes of memory.  The entry in the deferred queue (a
  std::vector) is the DeferredEvent instance itself (not a pointer to one elsewhere in heap) so still only 8 bytes per
  entry (and no heap fragmentation).  Even 100 backed up events (you'd have to have at least 100 sensors publishing
  because of dedup) would take up only 0.8 kB.
*/
struct DeferredEvent {
  friend class AsyncEventSourceResponse;

 protected:
  void *source_;
  message_generator_t *message_generator_;

 public:
  DeferredEvent(void *source, message_generator_t *message_generator)
      : source_(source), message_generator_(message_generator) {}
  bool operator==(const DeferredEvent &test) const {
    return (source_ == test.source_ && message_generator_ == test.message_generator_);
  }
} __attribute__((packed));

class AsyncEventSourceResponse {
  friend class AsyncEventSource;

 public:
  bool try_send_nodefer(const char *message, const char *event = nullptr, uint32_t id = 0, uint32_t reconnect = 0);
  void deferrable_send_state(void *source, const char *event_type, message_generator_t *message_generator);
  void loop();

 protected:
  AsyncEventSourceResponse(const AsyncWebServerRequest *request, esphome::web_server_idf::AsyncEventSource *server,
                           esphome::web_server::WebServer *ws);

  void deq_push_back_with_dedup_(void *source, message_generator_t *message_generator);
  void process_deferred_queue_();
  void process_buffer_();

  static void destroy(void *p);
  AsyncEventSource *server_;
  httpd_handle_t hd_{};
  int fd_{};
  std::vector<DeferredEvent> deferred_queue_;
  esphome::web_server::WebServer *web_server_;
  std::unique_ptr<esphome::web_server::ListEntitiesIterator> entities_iterator_;
  std::string event_buffer_{""};
  size_t event_bytes_sent_;
};

using AsyncEventSourceClient = AsyncEventSourceResponse;

class AsyncEventSource : public AsyncWebHandler {
  friend class AsyncEventSourceResponse;
  using connect_handler_t = std::function<void(AsyncEventSourceClient *)>;

 public:
  AsyncEventSource(std::string url, esphome::web_server::WebServer *ws) : url_(std::move(url)), web_server_(ws) {}
  ~AsyncEventSource() override;

  // NOLINTNEXTLINE(readability-identifier-naming)
  bool canHandle(AsyncWebServerRequest *request) override {
    return request->method() == HTTP_GET && request->url() == this->url_;
  }
  // NOLINTNEXTLINE(readability-identifier-naming)
  void handleRequest(AsyncWebServerRequest *request) override;
  // NOLINTNEXTLINE(readability-identifier-naming)
  void onConnect(connect_handler_t cb) { this->on_connect_ = std::move(cb); }

  void try_send_nodefer(const char *message, const char *event = nullptr, uint32_t id = 0, uint32_t reconnect = 0);
  void deferrable_send_state(void *source, const char *event_type, message_generator_t *message_generator);
  void loop();
  bool empty() { return this->count() == 0; }

  size_t count() const { return this->sessions_.size(); }

 protected:
  std::string url_;
  std::set<AsyncEventSourceResponse *> sessions_;
  connect_handler_t on_connect_{};
  esphome::web_server::WebServer *web_server_;
};

class DefaultHeaders {
  friend class AsyncWebServerRequest;
  friend class AsyncEventSourceResponse;

 public:
  // NOLINTNEXTLINE(readability-identifier-naming)
  void addHeader(const char *name, const char *value) { this->headers_.emplace_back(name, value); }

  // NOLINTNEXTLINE(readability-identifier-naming)
  static DefaultHeaders &Instance() {
    static DefaultHeaders instance;
    return instance;
  }

 protected:
  std::vector<std::pair<std::string, std::string>> headers_;
};

}  // namespace web_server_idf
}  // namespace esphome

using namespace esphome::web_server_idf;  // NOLINT(google-global-names-in-headers)

#endif  // !defined(USE_ESP_IDF)
