#pragma once

#ifdef USE_ARDUINO

#include <memory>
#include <utility>
#include "esphome/core/component.h"

#include <ESPAsyncWebServer.h>

namespace esphome {
namespace web_server_base {

namespace internal {

class MiddlewareHandler : public AsyncWebHandler {
 public:
  MiddlewareHandler(AsyncWebHandler *next) : next_(next) {}

  bool canHandle(AsyncWebServerRequest *request) override { return next_->canHandle(request); }
  void handleRequest(AsyncWebServerRequest *request) override { next_->handleRequest(request); }
  void handleUpload(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len,
                    bool final) override {
    next_->handleUpload(request, filename, index, data, len, final);
  }
  void handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) override {
    next_->handleBody(request, data, len, index, total);
  }
  bool isRequestHandlerTrivial() override { return next_->isRequestHandlerTrivial(); }

 protected:
  AsyncWebHandler *next_;
};

struct Credentials {
  std::string username;
  std::string password;
};

class AuthMiddlewareHandler : public MiddlewareHandler {
 public:
  AuthMiddlewareHandler(AsyncWebHandler *next, Credentials *credentials)
      : MiddlewareHandler(next), credentials_(credentials) {}

  bool check_auth(AsyncWebServerRequest *request) {
    bool success = request->authenticate(credentials_->username.c_str(), credentials_->password.c_str());
    if (!success) {
      request->requestAuthentication();
    }
    return success;
  }

  void handleRequest(AsyncWebServerRequest *request) override {
    if (!check_auth(request))
      return;
    MiddlewareHandler::handleRequest(request);
  }
  void handleUpload(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len,
                    bool final) override {
    if (!check_auth(request))
      return;
    MiddlewareHandler::handleUpload(request, filename, index, data, len, final);
  }
  void handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) override {
    if (!check_auth(request))
      return;
    MiddlewareHandler::handleBody(request, data, len, index, total);
  }

 protected:
  Credentials *credentials_;
};

}  // namespace internal

class WebServerBase : public Component {
 public:
  void init() {
    if (this->initialized_) {
      this->initialized_++;
      return;
    }
    this->server_ = std::make_shared<AsyncWebServer>(this->port_);
    this->server_->begin();

    for (auto *handler : this->handlers_)
      this->server_->addHandler(handler);

    this->initialized_++;
  }
  void deinit() {
    this->initialized_--;
    if (this->initialized_ == 0) {
      this->server_ = nullptr;
    }
  }
  std::shared_ptr<AsyncWebServer> get_server() const { return server_; }
  float get_setup_priority() const override;

  void set_auth_username(std::string auth_username) { credentials_.username = std::move(auth_username); }
  void set_auth_password(std::string auth_password) { credentials_.password = std::move(auth_password); }

  void add_handler(AsyncWebHandler *handler);

  void add_ota_handler();

  void set_port(uint16_t port) { port_ = port; }
  uint16_t get_port() const { return port_; }

 protected:
  friend class OTARequestHandler;

  int initialized_{0};
  uint16_t port_{80};
  std::shared_ptr<AsyncWebServer> server_{nullptr};
  std::vector<AsyncWebHandler *> handlers_;
  internal::Credentials credentials_;
};

class OTARequestHandler : public AsyncWebHandler {
 public:
  OTARequestHandler(WebServerBase *parent) : parent_(parent) {}
  void handleRequest(AsyncWebServerRequest *request) override;
  void handleUpload(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len,
                    bool final) override;
  bool canHandle(AsyncWebServerRequest *request) override {
    return request->url() == "/update" && request->method() == HTTP_POST;
  }

  bool isRequestHandlerTrivial() override { return false; }

 protected:
  uint32_t last_ota_progress_{0};
  uint32_t ota_read_length_{0};
  WebServerBase *parent_;
};

}  // namespace web_server_base
}  // namespace esphome

#endif  // USE_ARDUINO
