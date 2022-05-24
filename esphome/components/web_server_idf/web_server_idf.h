#pragma once
#ifdef USE_ESP_IDF

#include <string>
#include <functional>
#include <vector>
#include <map>
#include <set>
#include <esp_http_server.h>

namespace esphome {
namespace web_server_idf {

#define F(string_literal) (string_literal)
#define PGM_P const char *

#define strncpy_P strncpy

#include <cmath>
using std::isnan;

typedef std::string String;

class arduino_string {
 public:
  arduino_string(const std::string &s) : s_(s) {}
  const char *c_str() const { return this->s_.c_str(); }
  operator const std::string &() const { return this->s_; }

  float toFloat() const;

 protected:
  std::string s_;
};

class AsyncWebParameter {
 public:
  AsyncWebParameter(const std::string &name, const std::string &value, bool form = false, bool file = false,
                    size_t size = 0)
      : _name(name), _value(value), _size(size), _isForm(form), _isFile(file) {}
  const std::string &name() const { return _name; }
  const arduino_string &value() const { return _value; }
  size_t size() const { return _size; }
  bool isPost() const { return _isForm; }
  bool isFile() const { return _isFile; }

 protected:
  std::string _name;
  arduino_string _value;
  size_t _size;
  bool _isForm;
  bool _isFile;
};

class AsyncWebServerRequest;

class AsyncResponse {
 public:
  AsyncResponse(const AsyncWebServerRequest *req) : req_(req) {}
  virtual ~AsyncResponse() {}

  void addHeader(const char *name, const char *value);

  virtual const char *get_content_data() const = 0;
  virtual size_t get_content_size() const = 0;

 protected:
  const AsyncWebServerRequest *req_;
};

class AsyncEmptyResponse : public AsyncResponse {
 public:
  AsyncEmptyResponse(const AsyncWebServerRequest *req) : AsyncResponse(req) {}

  void addHeader(const char *name, const char *value);

  const char *get_content_data() const override { return nullptr; };
  size_t get_content_size() const override { return 0; };
};

class AsyncWebServerResponse : public AsyncResponse {
 public:
  AsyncWebServerResponse(const AsyncWebServerRequest *req, const std::string &content)
      : AsyncResponse(req), content_(content) {}

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
  const std::string url() const;
  size_t contentLength() const { return this->req_->content_len; }

  bool authenticate(const char *username, const char *password, const char *realm = nullptr,
                    bool passwordIsHash = false) const;
  void requestAuthentication(const char *realm = nullptr, bool isDigest = true) const;

  void send(AsyncResponse *response);
  void send(int code, const char *contentType = nullptr, const char *content = nullptr);
  AsyncEmptyResponse *beginResponse(int code, const char *contentType) {
    auto res = new AsyncEmptyResponse(this);
    this->init_response_(res, 200, contentType);
    return res;
  }
  AsyncWebServerResponse *beginResponse(int code, const char *contentType, const std::string &content) {
    auto res = new AsyncWebServerResponse(this, content);
    this->init_response_(res, code, contentType);
    return res;
  }
  AsyncProgmemResponse *beginResponse_P(int code, const char *contentType, const uint8_t *data,
                                        const size_t data_size) {
    auto res = new AsyncProgmemResponse(this, data, data_size);
    this->init_response_(res, code, contentType);
    return res;
  }
  AsyncResponseStream *beginResponseStream(const char *contentType) {
    auto res = new AsyncResponseStream(this);
    this->init_response_(res, 200, contentType);
    return res;
  }

  bool hasParam(const std::string &name, bool post = false, bool file = false);
  AsyncWebParameter *getParam(const std::string &name, bool post = false, bool file = false);

  operator httpd_req_t *() const { return this->req_; }

 protected:
  httpd_req_t *req_;
  AsyncResponse *rsp_{};
  std::map<std::string, AsyncWebParameter *> params_;
  AsyncWebServerRequest(httpd_req_t *req) : req_(req) {}
  void init_response_(AsyncResponse *rsp, int code, const char *contentType);
};

class AsyncWebHandler;

class AsyncWebServer {
 protected:
  uint16_t port_{};
  httpd_handle_t server_{};
  static esp_err_t request_handler_(httpd_req_t *r);
  std::vector<AsyncWebHandler *> handlers_;

 public:
  AsyncWebServer(uint16_t port) : port_(port){};
  ~AsyncWebServer() { this->end(); }

  void begin();
  void end();

  AsyncWebHandler &addHandler(AsyncWebHandler *handler) {
    this->handlers_.push_back(handler);
    return *handler;
  }
};

class AsyncWebHandler {
 public:
  virtual ~AsyncWebHandler() {}
  virtual bool canHandle(AsyncWebServerRequest *request) { return false; }
  virtual void handleRequest(AsyncWebServerRequest *request) {}
  virtual void handleUpload(AsyncWebServerRequest *request, const std::string &filename, size_t index, uint8_t *data,
                            size_t len, bool final) {}
  virtual void handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {}
  virtual bool isRequestHandlerTrivial() { return true; }
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

typedef AsyncEventSourceResponse AsyncEventSourceClient;

class AsyncEventSource : public AsyncWebHandler {
  friend class AsyncEventSourceResponse;
  typedef std::function<void(AsyncEventSourceClient *client)> connect_handler_t;

 public:
  AsyncEventSource(const std::string &url) : url_(url) {}
  virtual ~AsyncEventSource();

  bool canHandle(AsyncWebServerRequest *request) override {
    return request->method() == HTTP_GET && request->url() == this->url_;
  }
  void handleRequest(AsyncWebServerRequest *request) override;
  void onConnect(connect_handler_t cb) { this->on_connect_ = cb; }

  void send(const char *message, const char *event = nullptr, uint32_t id = 0, uint32_t reconnect = 0);

 protected:
  std::string url_;
  std::set<AsyncEventSourceResponse *> sessions_;
  connect_handler_t on_connect_{};
};

}  // namespace webserveridf
}  // namespace esphome

#endif
