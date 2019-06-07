#pragma once

#include <DNSServer.h>
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/components/web_server_base/web_server_base.h"

namespace esphome {

namespace captive_portal {

struct CaptivePortalSettings {
  char ssid[33];
  char password[65];
} PACKED;

class CaptivePortal : public AsyncWebHandler {
 public:
  CaptivePortal(web_server_base::WebServerBase *base) : base_(base) {}
  void setup();
  void loop() {
    if (this->dns_server_ != nullptr)
      this->dns_server_->processNextRequest();
  }
  void start();
  bool is_active() const { return this->active_; }
  void end() {
    this->active_ = false;
    this->base_->deinit();
    this->dns_server_->stop();
    delete this->dns_server_;
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
    }

    if (request->url() == "/wifisave" && request->method() == HTTP_POST)
      return true;

    return false;
  }

  void handle_index(AsyncWebServerRequest *request, bool save);

  void handle_wifisave(AsyncWebServerRequest *request);

  void handleRequest(AsyncWebServerRequest *req) override;

 protected:
  void override_sta(const std::string &ssid, const std::string &password);

  web_server_base::WebServerBase *base_;
  bool initialized_{false};
  bool active_{false};
  ESPPreferenceObject pref_;
  DNSServer *dns_server_{nullptr};
};

}  // namespace captive_portal
}  // namespace esphome
