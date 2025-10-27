#pragma once
#include "esphome/core/defines.h"
#ifdef USE_CAPTIVE_PORTAL
#include <memory>
#ifdef USE_ARDUINO
#include <DNSServer.h>
#endif
#ifdef USE_ESP_IDF
#include "dns_server_esp32_idf.h"
#endif
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
#ifdef USE_ARDUINO
    if (this->dns_server_ != nullptr) {
      this->dns_server_->processNextRequest();
    }
#endif
#ifdef USE_ESP_IDF
    if (this->dns_server_ != nullptr) {
      this->dns_server_->process_next_request();
    }
#endif
  }
  float get_setup_priority() const override;
  void start();
  bool is_active() const { return this->active_; }
  void end() {
    this->active_ = false;
    this->disable_loop();  // Stop processing DNS requests
    this->base_->deinit();
    if (this->dns_server_ != nullptr) {
      this->dns_server_->stop();
      this->dns_server_ = nullptr;
    }
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
#if defined(USE_ARDUINO) || defined(USE_ESP_IDF)
  std::unique_ptr<DNSServer> dns_server_{nullptr};
#endif
};

extern CaptivePortal *global_captive_portal;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace captive_portal
}  // namespace esphome
#endif
