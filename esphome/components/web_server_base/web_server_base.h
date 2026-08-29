#pragma once
#include "esphome/core/defines.h"
#if defined(USE_NETWORK) && !defined(USE_ZEPHYR)
#include <vector>

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

namespace esphome::web_server_base {

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
// All fields point to string literals in generated code; nothing is copied.
struct Credentials {
#if USE_ESP32 || defined(USE_WEBSERVER_AUTH_DIGEST)
  const char *username{nullptr};
  const char *password{nullptr};
  bool is_set() const { return username != nullptr; }
#else
  // base64("username:password"), precomputed at codegen time. Used by every non-ESP32 basic
  // auth build. The ESP8266 and RP2040 core libb64 wraps base64 output every 72 chars, so
  // letting the library encode and compare fails for long credentials; instead the header
  // payload is compared against this hash.
  const char *basic_auth_hash{nullptr};
  bool is_set() const { return basic_auth_hash != nullptr; }
#endif
};

class AuthMiddlewareHandler : public MiddlewareHandler {
 public:
  AuthMiddlewareHandler(AsyncWebHandler *next, Credentials *credentials)
      : MiddlewareHandler(next), credentials_(credentials) {}

  bool check_auth(AsyncWebServerRequest *request) {
    // The scheme is chosen at build time (USE_WEBSERVER_AUTH_DIGEST); the unused path is
    // compiled out. On ESP32 our own server picks the scheme internally.
#if USE_ESP32 || defined(USE_WEBSERVER_AUTH_DIGEST)
    bool success = request->authenticate(credentials_->username, credentials_->password);
#else
    bool success = request->authenticate(credentials_->basic_auth_hash);
#endif
    if (!success) {
#if USE_ESP32
      request->requestAuthentication();
#elif defined(USE_WEBSERVER_AUTH_DIGEST)
      request->requestAuthentication(nullptr, true);
#else
      request->requestAuthentication(nullptr, false);
#endif
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

class WebServerBase final {
 public:
  // The AsyncWebServer is created once and intentionally never deleted: on Arduino
  // platforms ESPAsyncWebServer owns its registered handlers, so destroying it would
  // also destroy live components (e.g. the captive portal) out from under us.
  // init()/deinit() refcount users and start/stop the listener; handlers are
  // registered once at creation and survive listener restarts.
  void init() {
    this->initialized_++;
    if (this->server_ != nullptr) {
      if (this->initialized_ == 1) {
        // Restart the listener after a previous deinit()
        this->server_->begin();
      }
      return;
    }
    this->server_ = new AsyncWebServer(this->port_);
    // All content is controlled and created by user - so allowing all origins is fine here.
    // NOTE: Currently 1 header. If more are added, update in __init__.py:
    //   cg.add_define("WEB_SERVER_DEFAULT_HEADERS_COUNT", 1)
    DefaultHeaders::Instance().addHeader(ESPHOME_F("Access-Control-Allow-Origin"), ESPHOME_F("*"));
    this->server_->begin();

    for (auto *handler : this->handlers_)
      this->server_->addHandler(handler);
  }
  void deinit() {
    if (this->initialized_ == 0)
      return;  // unbalanced deinit()
    this->initialized_--;
    if (this->initialized_ == 0) {
      this->server_->end();
    }
  }
  AsyncWebServer *get_server() const { return this->server_; }

#ifdef USE_WEBSERVER_AUTH
#if USE_ESP32 || defined(USE_WEBSERVER_AUTH_DIGEST)
  void set_auth_username(const char *auth_username) { credentials_.username = auth_username; }
  void set_auth_password(const char *auth_password) { credentials_.password = auth_password; }
#else
  void set_auth_basic_hash(const char *hash) { credentials_.basic_auth_hash = hash; }
#endif
#endif

  void add_handler(AsyncWebHandler *handler);
  /**
   * WARNING: Registers a handler that bypasses the USE_WEBSERVER_AUTH middleware.
   *
   * This should only be used for endpoints that are intentionally unauthenticated
   * (for example, captive portal or very limited-status endpoints). For normal
   * endpoints that should respect web server authentication, use add_handler().
   */
  void add_handler_without_auth(AsyncWebHandler *handler);

  void set_port(uint16_t port) { port_ = port; }
  uint16_t get_port() const { return port_; }

 protected:
  uint8_t initialized_{0};
  uint16_t port_{80};
  AsyncWebServer *server_{nullptr};
  std::vector<AsyncWebHandler *> handlers_;
#ifdef USE_WEBSERVER_AUTH
  internal::Credentials credentials_;
#endif
};

}  // namespace esphome::web_server_base
#endif  // USE_NETWORK && !USE_ZEPHYR
