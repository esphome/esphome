#pragma once
#include "esphome/core/defines.h"
#ifdef USE_NETWORK
#include <memory>
#include <utility>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/core/progmem.h"

#if USE_ESP32
#include "esphome/core/hal.h"
#include "esphome/components/web_server_idf/web_server_idf.h"
#else
#include <ESPAsyncWebServer.h>
#endif

#if USE_ESP32
using PlatformString = std::string;
#elif USE_ARDUINO
using PlatformString = String;
#endif

namespace esphome {
namespace web_server_base {

class WebServerBase;
extern WebServerBase *global_web_server_base;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

namespace internal {

class MiddlewareHandler : public AsyncWebHandler {
 public:
  MiddlewareHandler(AsyncWebHandler *next) : next_(next) {}

  bool canHandle(AsyncWebServerRequest *request) const override { return next_->canHandle(request); }
  void handleRequest(AsyncWebServerRequest *request) override { next_->handleRequest(request); }
  void handleUpload(AsyncWebServerRequest *request, const PlatformString &filename, size_t index, uint8_t *data,
                    size_t len, bool final) override {
    next_->handleUpload(request, filename, index, data, len, final);
  }
  void handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) override {
    next_->handleBody(request, data, len, index, total);
  }
  bool isRequestHandlerTrivial() const override { return next_->isRequestHandlerTrivial(); }

 protected:
  AsyncWebHandler *next_;
};

#ifdef USE_WEBSERVER_AUTH
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
  void handleUpload(AsyncWebServerRequest *request, const PlatformString &filename, size_t index, uint8_t *data,
                    size_t len, bool final) override {
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
#endif

}  // namespace internal

class WebServerBase : public Component {
 public:
  void init() {
    if (this->initialized_) {
      this->initialized_++;
      return;
    }
    this->server_ = std::make_unique<AsyncWebServer>(this->port_);
    // All content is controlled and created by user - so allowing all origins is fine here.
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
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
  AsyncWebServer *get_server() const { return this->server_.get(); }
  float get_setup_priority() const override;

#ifdef USE_WEBSERVER_AUTH
  void set_auth_username(std::string auth_username) { credentials_.username = std::move(auth_username); }
  void set_auth_password(std::string auth_password) { credentials_.password = std::move(auth_password); }
#endif

  void add_handler(AsyncWebHandler *handler);

  void set_port(uint16_t port) { port_ = port; }
  uint16_t get_port() const { return port_; }

 protected:
  int initialized_{0};
  uint16_t port_{80};
  std::unique_ptr<AsyncWebServer> server_{nullptr};
  std::vector<AsyncWebHandler *> handlers_;
#ifdef USE_WEBSERVER_AUTH
  internal::Credentials credentials_;
#endif
};

}  // namespace web_server_base
}  // namespace esphome
#endif
