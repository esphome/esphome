#pragma once
#ifdef USE_ESP_IDF

#include <esp_http_server.h>

#include <string>
#include <functional>
#include <vector>
#include <map>
#include <set>

namespace esphome {
namespace web_server_idf {

#define F(string_literal) (string_literal)
#define PGM_P const char *

#define strncpy_P strncpy

#include <cmath>
using std::isnan;

using String = std::string;

class AsyncWebParameter {
 public:
  AsyncWebParameter(std::string value) : value_(std::move(value)) {}
  const std::string &value() const { return this->value_; }

 protected:
  std::string value_;
};

class AsyncWebServerRequest;

class AsyncResponse {
 public:
  AsyncResponse(const AsyncWebServerRequest *req) : req_(req) {}
  virtual ~AsyncResponse() {}

  void addHeader /*NOLINT(readability-identifier-naming)*/ (const char *name, const char *value);

  virtual const char *get_content_data() const = 0;
  virtual size_t get_content_size() const = 0;

 protected:
  const AsyncWebServerRequest *req_;
};

class AsyncEmptyResponse : public AsyncResponse {
 public:
  AsyncEmptyResponse(const AsyncWebServerRequest *req) : AsyncResponse(req) {}

  const char *get_content_data() const override { return nullptr; };
  size_t get_content_size() const override { return 0; };
};

class AsyncWebServerResponse : public AsyncResponse {
 public:
  AsyncWebServerResponse(const AsyncWebServerRequest *req, std::string content)
      : AsyncResponse(req), content_(std::move(content)) {}

  const char *get_content_data() const override { return this->content_.c_str(); };
  size_t get_content_size() const override { return this->content_.size(); };

 protected:
  std::string content_;
};

class AsyncResponseStream : public AsyncResponse {
 public:
  AsyncResponseStream(const AsyncWebServerRequest *req) : AsyncResponse(req) {}

  const char *get_content_data() const override { return this->content_.c_str(); };
  size_t get_content_size() const override { return this->content_.size(); };

  void print(const char *str) { this->content_.append(str); }
  void print(const std::string &str) { this->content_.append(str); }
  void print(float value);

 protected:
  std::string content_;
};

class AsyncProgmemResponse : public AsyncResponse {
 public:
  AsyncProgmemResponse(const AsyncWebServerRequest *req, const uint8_t *data, const size_t size)
      : AsyncResponse(req), data_(data), size_(size) {}

  const char *get_content_data() const override { return reinterpret_cast<const char *>(this->data_); };
  size_t get_content_size() const override { return this->size_; };

 protected:
  const uint8_t *data_;
  const size_t size_;
};

class AsyncWebServerRequest {
  friend class AsyncWebServerResponse;
  friend class AsyncWebServer;

 public:
  ~AsyncWebServerRequest();

  http_method method() const { return static_cast<http_method>(this->req_->method); }
  std::string url() const;
  size_t contentLength /*NOLINT(readability-identifier-naming)*/ () const { return this->req_->content_len; }

  bool authenticate(const char *username, const char *password) const;
  void requestAuthentication /*NOLINT(readability-identifier-naming)*/ (const char *realm = nullptr) const;

  void send(AsyncResponse *response);
  void send(int code, const char *content_type = nullptr, const char *content = nullptr);
  AsyncEmptyResponse *beginResponse /*NOLINT(readability-identifier-naming)*/ (int code, const char *content_type) {
    auto *res = new AsyncEmptyResponse(this);  // NOLINT(cppcoreguidelines-owning-memory)
    this->init_response_(res, 200, content_type);
    return res;
  }
  AsyncWebServerResponse *beginResponse /*NOLINT(readability-identifier-naming)*/ (int code, const char *content_type,
                                                                                   const std::string &content) {
    auto *res = new AsyncWebServerResponse(this, content);  // NOLINT(cppcoreguidelines-owning-memory)
    this->init_response_(res, code, content_type);
    return res;
  }
  AsyncProgmemResponse *beginResponse_P /*NOLINT(readability-identifier-naming)*/ (int code, const char *content_type,
                                                                                   const uint8_t *data,
                                                                                   const size_t data_size) {
    auto *res = new AsyncProgmemResponse(this, data, data_size);  // NOLINT(cppcoreguidelines-owning-memory)
    this->init_response_(res, code, content_type);
    return res;
  }
  AsyncResponseStream *beginResponseStream /*NOLINT(readability-identifier-naming)*/ (const char *content_type) {
    auto *res = new AsyncResponseStream(this);  // NOLINT(cppcoreguidelines-owning-memory)
    this->init_response_(res, 200, content_type);
    return res;
  }

  bool hasParam /*NOLINT(readability-identifier-naming)*/ (const std::string &name);
  AsyncWebParameter *getParam /*NOLINT(readability-identifier-naming)*/ (const std::string &name);

  operator httpd_req_t *() const { return this->req_; }

 protected:
  httpd_req_t *req_;
  AsyncResponse *rsp_{};
  std::map<std::string, AsyncWebParameter *> params_;
  AsyncWebServerRequest(httpd_req_t *req) : req_(req) {}
  void init_response_(AsyncResponse *rsp, int code, const char *content_type);
};

class AsyncWebHandler;

class AsyncWebServer {
 protected:
  uint16_t port_{};
  httpd_handle_t server_{};
  static esp_err_t request_handler(httpd_req_t *r);
  std::vector<AsyncWebHandler *> handlers_;

 public:
  AsyncWebServer(uint16_t port) : port_(port){};
  ~AsyncWebServer() { this->end(); }

  void begin();
  void end();

  AsyncWebHandler &addHandler /*NOLINT(readability-identifier-naming)*/ (AsyncWebHandler *handler) {
    this->handlers_.push_back(handler);
    return *handler;
  }
};

class AsyncWebHandler {
 public:
  virtual ~AsyncWebHandler() {}
  virtual bool canHandle /*NOLINT(readability-identifier-naming)*/ (AsyncWebServerRequest *request) { return false; }
  virtual void handleRequest /*NOLINT(readability-identifier-naming)*/ (AsyncWebServerRequest *request) {}
  virtual void handleUpload /*NOLINT(readability-identifier-naming)*/ (AsyncWebServerRequest *request,
                                                                       const std::string &filename, size_t index,
                                                                       uint8_t *data, size_t len, bool final) {}
  virtual void handleBody /*NOLINT(readability-identifier-naming)*/ (AsyncWebServerRequest *request, uint8_t *data,
                                                                     size_t len, size_t index, size_t total) {}
  virtual bool isRequestHandlerTrivial /*NOLINT(readability-identifier-naming)*/ () { return true; }
};

class AsyncEventSource;

class AsyncEventSourceResponse {
  friend class AsyncEventSource;

 public:
  void send(const char *message, const char *event = nullptr, uint32_t id = 0, uint32_t reconnect = 0);

 protected:
  AsyncEventSourceResponse(const AsyncWebServerRequest *request, AsyncEventSource *server);
  static void destroy(void *p);
  AsyncEventSource *server_;
  httpd_handle_t hd_{};
  int fd_{};
};

using AsyncEventSourceClient = AsyncEventSourceResponse;

class AsyncEventSource : public AsyncWebHandler {
  friend class AsyncEventSourceResponse;
  using connect_handler_t = std::function<void(AsyncEventSourceClient *)>;

 public:
  AsyncEventSource(std::string url) : url_(std::move(url)) {}
  ~AsyncEventSource() override;

  bool canHandle /*NOLINT(readability-identifier-naming)*/ (AsyncWebServerRequest *request) override {
    return request->method() == HTTP_GET && request->url() == this->url_;
  }
  void handleRequest /*NOLINT(readability-identifier-naming)*/ (AsyncWebServerRequest *request) override;
  void onConnect /*NOLINT(readability-identifier-naming)*/ (connect_handler_t cb) { this->on_connect_ = std::move(cb); }

  void send(const char *message, const char *event = nullptr, uint32_t id = 0, uint32_t reconnect = 0);

 protected:
  std::string url_;
  std::set<AsyncEventSourceResponse *> sessions_;
  connect_handler_t on_connect_{};
};

}  // namespace web_server_idf
}  // namespace esphome

#endif
