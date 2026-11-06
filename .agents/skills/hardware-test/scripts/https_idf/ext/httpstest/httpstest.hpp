#pragma once
#include "esphome/core/component.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include <esp_heap_caps.h>
#include <cstring>
#include <iterator>

// Written by make_certs.sh: CERT, KEY and BAD_KEY as PEM strings.
#include "certs.h"

namespace esphome::httpstest {

// Listener per mode number: the switch path, the key (nullptr for plain HTTP) and the port.
struct ListenerMode {
  const char *path;
  const char *key;
  uint16_t port;
};
static constexpr uint32_t LATER_SWITCH_ID = 0;
static constexpr uint32_t LATER_SWITCH_MS = 3000;
static constexpr ListenerMode MODES[] = {
    {"/hw/switch/http", nullptr, 80},
    {"/hw/switch/https", KEY, 443},
    {"/hw/switch/bad", BAD_KEY, 443},
    {"/hw/switch/https8443", KEY, 8443},
};

// Serves /hw over HTTPS (port 443 or 8443) or HTTP (port 80) and switches between them from the main loop.
// /hw/switch/{http,https,https8443,bad} picks the listener; bad sets a key that does not match the certificate.
// A /later suffix runs the switch 3 s after the request, so a test can open a stalled client before it runs.
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
      if (big[0] == '\0')
        memset(big, 'x', sizeof(big) - 1);
      request->send(200, "text/plain", big);
      return;
    }
    for (int mode = 0; mode < (int) std::size(MODES); mode++) {
      if (!url.starts_with(MODES[mode].path))
        continue;
      size_t len = strlen(MODES[mode].path);
      StringRef rest(url.c_str() + len, url.size() - len);
      // the setters restart the listener, so run them from the main loop
      if (rest.empty()) {
        this->defer([this, mode]() { this->switch_(mode); });
      } else if (rest == "/later") {
        this->set_timeout(LATER_SWITCH_ID, LATER_SWITCH_MS, [this, mode]() { this->switch_(mode); });
      } else {
        continue;
      }
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
    const ListenerMode &m = MODES[mode];
    this->base_->set_ssl_credentials(m.key != nullptr ? CERT : nullptr, m.key, m.port);
    this->mode_ = mode;
    ESP_LOGI("hwt", "SWITCH to mode %d took %u ms", mode, (unsigned) (millis() - t0));
  }

  web_server_base::WebServerBase *base_;
  uint32_t requests_{0};
  int mode_{1};
};

}  // namespace esphome::httpstest
