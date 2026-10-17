#pragma once
#include "esphome/core/defines.h"
#ifdef USE_CAPTIVE_PORTAL
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/components/web_server_base/captive_dns.h"

namespace esphome::captive_portal {

class CaptivePortal final : public AsyncWebHandler, public Component {
 public:
  CaptivePortal(web_server_base::WebServerBase *base);
  void setup() override;
  void dump_config() override;
  void loop() override { this->dns_.loop(); }
  float get_setup_priority() const override;
  void start();
  bool is_active() const { return this->active_; }
  void end() {
    this->active_ = false;
    this->disable_loop();  // Stop processing DNS requests
    this->base_->deinit();
    this->dns_.stop();
  }

  bool canHandle(AsyncWebServerRequest *request) const override {
    // Handle all GET requests when captive portal is active
    // This allows us to respond with the portal page for any URL,
    // triggering OS captive portal detection
    return this->active_ && request->method() == HTTP_GET;
  }

  void handle_config(AsyncWebServerRequest *request);

  void handle_wifisave(AsyncWebServerRequest *request);

  void handleRequest(AsyncWebServerRequest *req) override;

 protected:
  web_server_base::WebServerBase *base_;
  bool initialized_{false};
  bool active_{false};
  web_server_base::CaptiveDNS dns_;
};

extern CaptivePortal *global_captive_portal;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::captive_portal

#endif
