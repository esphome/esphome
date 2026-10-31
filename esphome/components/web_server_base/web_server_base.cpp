#include "web_server_base.h"
#if defined(USE_NETWORK) && !defined(USE_ZEPHYR) && !defined(USE_HOST)

namespace esphome::web_server_base {

WebServerBase *global_web_server_base = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void WebServerBase::add_handler(AsyncWebHandler *handler) {
#ifdef USE_WEBSERVER_AUTH
  if (credentials_.is_set()) {
    handler = new internal::AuthMiddlewareHandler(handler, &credentials_);
  }
#endif
  this->add_handler_without_auth(handler);
}

void WebServerBase::add_handler_without_auth(AsyncWebHandler *handler) {
  if (this->server_ == nullptr) {
#ifdef USE_WEBSERVER_BASE_PENDING_HANDLER
    if (this->pending_handler_ == nullptr) {
      this->pending_handler_ = handler;
      return;
    }
#endif
    // Nowhere for it to wait: create the server now, after the waiting handler to keep the order
    this->create_server_();
  }
  this->server_->addHandler(handler);
}

void WebServerBase::create_server_() {
  this->server_ = new AsyncWebServer(this->port_);
  // All content is controlled and created by user - so allowing all origins is fine here.
  // NOTE: Currently 1 header. If more are added, update in __init__.py:
  //   cg.add_define("WEB_SERVER_DEFAULT_HEADERS_COUNT", 1)
  DefaultHeaders::Instance().addHeader(ESPHOME_F("Access-Control-Allow-Origin"), ESPHOME_F("*"));
#ifdef USE_WEBSERVER_BASE_PENDING_HANDLER
  if (this->pending_handler_ != nullptr)
    this->server_->addHandler(this->pending_handler_);
#endif
}

}  // namespace esphome::web_server_base
#endif
