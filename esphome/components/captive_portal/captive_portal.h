#pragma once

#ifdef USE_ARDUINO

#include <memory>
#include <DNSServer.h>
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include "esphome/components/web_server_base/web_server_base.h"

namespace esphome {

namespace captive_portal {

class CaptivePortal : public AsyncWebHandler, public Component {
 public:
  CaptivePortal(web_server_base::WebServerBase *base);
  void setup() override;
  void dump_config() override;
  void loop() override {
    if (this->dns_server_ != nullptr)
      this->dns_server_->processNextRequest();
  }
  float get_setup_priority() const override;
  void start();
  bool is_active() const { return this->active_; }
  void end() {
    this->active_ = false;
    this->base_->deinit();
    this->dns_server_->stop();
    this->dns_server_ = nullptr;
  }

  bool canHandle(AsyncWebServerRequest *request) override {
    if (!this->active_)
      return false;

    if (request->method() == HTTP_GET) {
      if (request->url() == "/")
        return true;
      if (request->url() == "/stylesheet.css")
        return true;
      if (request->url() == "/wifi-strength-1.svg")
        return true;
      if (request->url() == "/wifi-strength-2.svg")
        return true;
      if (request->url() == "/wifi-strength-3.svg")
        return true;
      if (request->url() == "/wifi-strength-4.svg")
        return true;
      if (request->url() == "/lock.svg")
        return true;
      if (request->url() == "/wifisave")
        return true;
    }

    return false;
  }

  void handle_index(AsyncWebServerRequest *request);

  void handle_wifisave(AsyncWebServerRequest *request);

  void handleRequest(AsyncWebServerRequest *req) override;

 protected:
  web_server_base::WebServerBase *base_;
  bool initialized_{false};
  bool active_{false};
  std::unique_ptr<DNSServer> dns_server_{nullptr};
};

extern CaptivePortal *global_captive_portal;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace captive_portal
}  // namespace esphome

#endif  // USE_ARDUINO
