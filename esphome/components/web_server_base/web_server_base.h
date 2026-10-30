#pragma once
#include "esphome/core/defines.h"
#if defined(USE_NETWORK) && !defined(USE_ZEPHYR) && !defined(USE_HOST)
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
#endif

// Wraps every handler added through WebServerBase::add_handler(): rejects cross-origin browser
// requests the origin policy does not allow, then checks credentials when auth is configured.
// It holds only the next handler; the policy lives in global_web_server_base.
class GuardMiddlewareHandler : public MiddlewareHandler {
 public:
  using MiddlewareHandler::MiddlewareHandler;

  void handleRequest(AsyncWebServerRequest *request) override;
  void handleUpload(AsyncWebServerRequest *request, const PlatformString &filename, size_t index, uint8_t *data,
                    size_t len, bool final) override;
  void handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) override;

 protected:
  // Upload and body chunks arrive before handleRequest(), which sends the 403 for a rejected origin.
  bool check_(AsyncWebServerRequest *request);
#ifdef USE_WEBSERVER_AUTH
  bool check_auth_(AsyncWebServerRequest *request);
#endif
};

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
#ifdef USE_WEBSERVER_ALLOWED_ORIGINS
  /** Set the extra origins browsers may make cross-origin requests from, for every handler added
   * through add_handler(). Requests without an Origin header and same-origin requests are always
   * allowed. A single "*" entry allows any origin; each other entry must exactly match the
   * browser's Origin header ("scheme://host[:port]").
   *
   * @param origins nullptr-terminated table of allowed origins; must outlive the server.
   */
  void set_allowed_origins(const char *const *origins) { this->allowed_origins_ = origins; }
#endif

  void add_handler(AsyncWebHandler *handler);
  /**
   * WARNING: Registers a handler that bypasses the auth and origin middleware.
   *
   * This should only be used for endpoints that are intentionally unauthenticated
   * (for example, captive portal or very limited-status endpoints). For normal
   * endpoints that should respect web server authentication, use add_handler().
   * The origin check is skipped too, so a captive portal-only build carries none of it;
   * the portal's own pages are same-origin and the OS probes send no Origin.
   */
  void add_handler_without_auth(AsyncWebHandler *handler);

  void set_port(uint16_t port) { port_ = port; }
  uint16_t get_port() const { return port_; }

 protected:
  uint8_t initialized_{0};
  uint16_t port_{80};  // Keep in sync with DEFAULT_PORT in web_server/__init__.py
  AsyncWebServer *server_{nullptr};
  std::vector<AsyncWebHandler *> handlers_;
#ifdef USE_WEBSERVER_AUTH
  internal::Credentials credentials_;
#endif
#ifdef USE_WEBSERVER_ALLOWED_ORIGINS
  const char *const *allowed_origins_{nullptr};
#endif

  friend class internal::GuardMiddlewareHandler;
  // Not inlined, so its header copies leave the stack before the wrapped handler runs
  bool __attribute__((noinline)) is_request_origin_allowed_(AsyncWebServerRequest *request) const;
};

}  // namespace esphome::web_server_base
#endif  // USE_NETWORK && !USE_ZEPHYR
