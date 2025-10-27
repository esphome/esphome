#include "web_server_base.h"
#ifdef USE_NETWORK
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace web_server_base {

static const char *const TAG = "web_server_base";

WebServerBase *global_web_server_base = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void WebServerBase::add_handler(AsyncWebHandler *handler) {
  // remove all handlers

#ifdef USE_WEBSERVER_AUTH
  if (!credentials_.username.empty()) {
    handler = new internal::AuthMiddlewareHandler(handler, &credentials_);
  }
#endif
  this->handlers_.push_back(handler);
  if (this->server_ != nullptr) {
    this->server_->addHandler(handler);
  }
}

float WebServerBase::get_setup_priority() const {
  // Before WiFi (captive portal)
  return setup_priority::WIFI + 2.0f;
}

}  // namespace web_server_base
}  // namespace esphome
#endif
