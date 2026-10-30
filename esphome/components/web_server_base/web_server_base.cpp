#include "web_server_base.h"
#if defined(USE_NETWORK) && !defined(USE_ZEPHYR) && !defined(USE_HOST)
#include "origin.h"

namespace esphome::web_server_base {

WebServerBase *global_web_server_base = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

#ifdef USE_ESP32
// Longest Origin or Host value the check reads, less the terminator; keep in sync with
// MAX_ORIGIN_LENGTH in web_server_base/__init__.py.
static constexpr size_t ORIGIN_HEADER_BUF_SIZE = 128;
#endif

bool WebServerBase::is_request_origin_allowed_(AsyncWebServerRequest *request) const {
#ifdef USE_WEBSERVER_ALLOWED_ORIGINS
  const char *const *allowed = this->allowed_origins_;
#else
  const char *const *allowed = nullptr;
#endif
#ifdef USE_ESP32
  // Stack copies: the IDF server only hands out header values by copy
  char origin[ORIGIN_HEADER_BUF_SIZE];
  const size_t origin_len = request->get_header_to("Origin", origin);
  if (origin_len == 0)
    return true;
  if (origin_len == SIZE_MAX)
    return allows_any_origin(allowed);
  char host[ORIGIN_HEADER_BUF_SIZE];
  size_t host_len = request->get_header_to("Host", host);
  if (host_len == SIZE_MAX)
    host_len = 0;
  return is_origin_allowed(origin, origin_len, host, host_len, allowed);
#else
  const AsyncWebHeader *origin = request->getHeader("Origin");
  if (origin == nullptr)
    return true;
  const AsyncWebHeader *host = request->getHeader("Host");
  return is_origin_allowed(origin->value().c_str(), origin->value().length(),
                           host != nullptr ? host->value().c_str() : nullptr,
                           host != nullptr ? host->value().length() : 0, allowed);
#endif
}

namespace internal {

bool GuardMiddlewareHandler::check_auth_(AsyncWebServerRequest *request) {
#ifdef USE_WEBSERVER_AUTH
  const Credentials &credentials = global_web_server_base->credentials_;
  if (!credentials.is_set())
    return true;
  // The scheme is chosen at build time (USE_WEBSERVER_AUTH_DIGEST); the unused path is
  // compiled out. On ESP32 our own server picks the scheme internally.
#if USE_ESP32 || defined(USE_WEBSERVER_AUTH_DIGEST)
  bool success = request->authenticate(credentials.username, credentials.password);
#else
  bool success = request->authenticate(credentials.basic_auth_hash);
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
#else
  return true;
#endif
}

bool GuardMiddlewareHandler::check_(AsyncWebServerRequest *request) {
  return global_web_server_base->is_request_origin_allowed_(request) && this->check_auth_(request);
}

void GuardMiddlewareHandler::handleRequest(AsyncWebServerRequest *request) {
  if (!global_web_server_base->is_request_origin_allowed_(request)) {
    request->send(403);
    return;
  }
  if (this->check_auth_(request))
    MiddlewareHandler::handleRequest(request);
}

void GuardMiddlewareHandler::handleUpload(AsyncWebServerRequest *request, const PlatformString &filename, size_t index,
                                          uint8_t *data, size_t len, bool final) {
  // Every chunk carries the first one's headers; the wrapped handler ignores chunks of a request
  // whose first chunk it never saw.
  if (index == 0 && !this->check_(request))
    return;
  MiddlewareHandler::handleUpload(request, filename, index, data, len, final);
}

void GuardMiddlewareHandler::handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index,
                                        size_t total) {
  if (index == 0 && !this->check_(request))
    return;
  MiddlewareHandler::handleBody(request, data, len, index, total);
}

}  // namespace internal

void WebServerBase::add_handler(AsyncWebHandler *handler) {
  this->add_handler_without_auth(new internal::GuardMiddlewareHandler(handler));
}

void WebServerBase::add_handler_without_auth(AsyncWebHandler *handler) {
  this->handlers_.push_back(handler);
  if (this->server_ != nullptr) {
    this->server_->addHandler(handler);
  }
}

}  // namespace esphome::web_server_base
#endif
