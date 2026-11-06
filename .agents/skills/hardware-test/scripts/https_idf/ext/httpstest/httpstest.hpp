#pragma once
#include "esphome/core/component.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include <esp_heap_caps.h>

// Written by make_certs.sh: CERT, KEY and BAD_KEY as PEM strings.
#include "certs.h"

namespace esphome::httpstest {

// Serves /hw over HTTPS (port 443 or 8443) or HTTP (port 80) and switches between them from the main loop.
// /hw/switch/{http,https,https8443,bad} picks the listener; bad sets a key that does not match the certificate.
class HttpsTest : public Component, public AsyncWebHandler {
 public:
  explicit HttpsTest(web_server_base::WebServerBase *base) : base_(base) {}
  float get_setup_priority() const override { return setup_priority::WIFI - 1.0f; }

  void setup() override {
    this->base_->add_handler(this);
    this->base_->set_ssl_credentials(CERT, KEY, 443);
    this->base_->init();
    ESP_LOGI("hwt", "HTTPS listener started on 443");
  }

  bool canHandle(AsyncWebServerRequest *request) const override {
    char buf[AsyncWebServerRequest::URL_BUF_SIZE];
    return request->url_to(buf).starts_with("/hw");
  }

  void handleRequest(AsyncWebServerRequest *request) override {
    char buf[AsyncWebServerRequest::URL_BUF_SIZE];
    auto url = request->url_to(buf);
    this->requests_++;
    if (url == "/hw/big") {
      static char big[16384];
      memset(big, 'x', sizeof(big) - 1);
      request->send(200, "text/plain", big);
      return;
    }
    int mode = -1;
    if (url == "/hw/switch/http")
      mode = 0;
    else if (url == "/hw/switch/https")
      mode = 1;
    else if (url == "/hw/switch/bad")
      mode = 2;
    else if (url == "/hw/switch/https8443")
      mode = 3;
    if (mode >= 0) {
      // the setters restart the listener, so run them from the main loop
      this->defer([this, mode]() { this->switch_(mode); });
      request->send(202, "text/plain", "switching");
      return;
    }
    char body[96];
    snprintf(body, sizeof(body), "ok mode=%d requests=%u heap=%u\n", this->mode_, (unsigned) this->requests_,
             (unsigned) heap_caps_get_free_size(MALLOC_CAP_8BIT));
    request->send(200, "text/plain", body);
  }

  void cycle() { this->switch_(this->mode_ == 1 ? 3 : 1); }

 protected:
  void switch_(int mode) {
    uint32_t t0 = millis();
    if (mode == 0)
      this->base_->set_ssl_credentials(nullptr, nullptr, 80);
    else if (mode == 1)
      this->base_->set_ssl_credentials(CERT, KEY, 443);
    else if (mode == 2)
      this->base_->set_ssl_credentials(CERT, BAD_KEY, 443);
    else
      this->base_->set_ssl_credentials(CERT, KEY, 8443);
    this->mode_ = mode;
    ESP_LOGI("hwt", "SWITCH to mode %d took %u ms", mode, (unsigned) (millis() - t0));
  }

  web_server_base::WebServerBase *base_;
  uint32_t requests_{0};
  int mode_{1};
};

}  // namespace esphome::httpstest
