#include "web_server_base.h"
#if defined(USE_NETWORK) && !defined(USE_ZEPHYR) && !defined(USE_HOST)
#include "origin.h"

namespace esphome::web_server_base {

WebServerBase *global_web_server_base = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

#ifdef USE_ESP32
// Longest Origin or Host value the check reads; web_server limits allowed_origins entries to fit.
static constexpr size_t ORIGIN_HEADER_BUF_SIZE = 128;

// Copy a header into buf. Returns its length, 0 when absent, or SIZE_MAX when it does not fit.
static size_t read_header(httpd_req_t *req, const char *name, char *buf) {
  const size_t len = httpd_req_get_hdr_value_len(req, name);
  if (len == 0)
    return 0;
  if (len >= ORIGIN_HEADER_BUF_SIZE || httpd_req_get_hdr_value_str(req, name, buf, ORIGIN_HEADER_BUF_SIZE) != ESP_OK)
    return SIZE_MAX;
  return len;
}
#endif

bool WebServerBase::is_request_origin_allowed_(AsyncWebServerRequest *request) const {
#ifdef USE_WEBSERVER_ALLOWED_ORIGINS
  const char *const *allowed = this->allowed_origins_;
#else
  const char *const *allowed = nullptr;
#endif
#ifdef USE_ESP32
  // Stack copies: the IDF server only hands out header values by copy
  httpd_req_t *req = *request;
  char origin[ORIGIN_HEADER_BUF_SIZE];
  const size_t origin_len = read_header(req, "Origin", origin);
  if (origin_len == 0)
    return true;
  if (origin_len == SIZE_MAX)
    return is_origin_allowed(nullptr, 1, nullptr, 0, allowed);
  char host[ORIGIN_HEADER_BUF_SIZE];
  size_t host_len = read_header(req, "Host", host);
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

bool GuardMiddlewareHandler::check_(AsyncWebServerRequest *request) {
  if (!global_web_server_base->is_request_origin_allowed_(request))
    return false;
#ifdef USE_WEBSERVER_AUTH
  return this->check_auth_(request);
#else
  return true;
#endif
}

#ifdef USE_WEBSERVER_AUTH
bool GuardMiddlewareHandler::check_auth_(AsyncWebServerRequest *request) {
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
}
#endif

void GuardMiddlewareHandler::handleRequest(AsyncWebServerRequest *request) {
  if (!global_web_server_base->is_request_origin_allowed_(request)) {
    request->send(403);
    return;
  }
#ifdef USE_WEBSERVER_AUTH
  if (!this->check_auth_(request))
    return;
#endif
  MiddlewareHandler::handleRequest(request);
}

void GuardMiddlewareHandler::handleUpload(AsyncWebServerRequest *request, const PlatformString &filename, size_t index,
                                          uint8_t *data, size_t len, bool final) {
  if (!this->check_(request))
    return;
  MiddlewareHandler::handleUpload(request, filename, index, data, len, final);
}

void GuardMiddlewareHandler::handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index,
                                        size_t total) {
  if (!this->check_(request))
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
