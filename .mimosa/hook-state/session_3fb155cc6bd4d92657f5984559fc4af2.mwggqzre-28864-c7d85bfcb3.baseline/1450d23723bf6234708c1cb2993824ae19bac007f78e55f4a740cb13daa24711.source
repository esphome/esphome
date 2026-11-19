#include "web_server_base.h"
#if defined(USE_NETWORK) && !defined(USE_ZEPHYR) && !defined(USE_HOST)

namespace esphome::web_server_base {

WebServerBase *global_web_server_base = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

#if defined(USE_WEBSERVER_ALLOWED_ORIGINS) || !defined(USE_WEBSERVER_ALLOW_ANY_ORIGIN)
// Not inlined, so its header copies leave the stack before the wrapped handler runs
static bool __attribute__((noinline)) is_request_origin_allowed(AsyncWebServerRequest *request,
                                                                const char *const *allowed) {
#ifdef USE_ESP32
  // Stack copies: the IDF server only hands out header values by copy
  char origin[MAX_ORIGIN_LENGTH + 1];
  const size_t origin_len = request->get_header_to("Origin", origin);
  if (origin_len == 0)
    return true;
  // Longer than codegen lets a configured entry be
  if (origin_len == SIZE_MAX)
    return false;
  char host[MAX_ORIGIN_LENGTH + 1];
  size_t host_len = request->get_header_to("Host", host);
  // A Host that does not fit means an Origin longer than the buffer, already handled above
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
#endif

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

bool GuardMiddlewareHandler::check_origin_(AsyncWebServerRequest *request) {
  // Codegen never sets both defines
#if defined(USE_WEBSERVER_ALLOWED_ORIGINS)
  return is_request_origin_allowed(request, global_web_server_base->allowed_origins_);
#elif defined(USE_WEBSERVER_ALLOW_ANY_ORIGIN)
  return true;
#else
  return is_request_origin_allowed(request, nullptr);
#endif
}

bool GuardMiddlewareHandler::check_chunk_(AsyncWebServerRequest *request, size_t index) {
  // Every chunk is gated, but only the first of a request pays for the header copies and digest.
  // A request always starts at index 0, so a new request reusing a freed request's address is
  // checked again; an interleaved request has another address; a rejected one never sets passed_.
  if (index != 0 && request == this->passed_)
    return true;
  if (!this->check_origin_(request) || !this->check_auth_(request)) {
    this->passed_ = nullptr;
    return false;
  }
  this->passed_ = request;
  return true;
}

void GuardMiddlewareHandler::handleRequest(AsyncWebServerRequest *request) {
  this->passed_ = nullptr;
  if (!this->check_origin_(request)) {
    request->send(403);
    return;
  }
  if (this->check_auth_(request))
    MiddlewareHandler::handleRequest(request);
}

void GuardMiddlewareHandler::handleUpload(AsyncWebServerRequest *request, const PlatformString &filename, size_t index,
                                          uint8_t *data, size_t len, bool final) {
  // A handler that buffers chunks never sees one from a rejected request
  if (!this->check_chunk_(request, index))
    return;
  MiddlewareHandler::handleUpload(request, filename, index, data, len, final);
}

void GuardMiddlewareHandler::handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index,
                                        size_t total) {
  if (!this->check_chunk_(request, index))
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
