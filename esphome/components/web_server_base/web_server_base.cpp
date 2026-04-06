#include "web_server_base.h"
#ifdef USE_NETWORK

namespace esphome::web_server_base {

WebServerBase *global_web_server_base = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void WebServerBase::add_handler(AsyncWebHandler *handler) {
#ifdef USE_WEBSERVER_AUTH
  if (!credentials_.username.empty()) {
    handler = new internal::AuthMiddlewareHandler(handler, &credentials_);
  }
#endif
  this->add_handler_without_auth(handler);
}

void WebServerBase::add_handler_without_auth(AsyncWebHandler *handler) {
  this->handlers_.push_back(handler);
  if (this->server_ != nullptr) {
    this->server_->addHandler(handler);
  }
}

}  // namespace esphome::web_server_base
#endif
