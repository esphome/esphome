#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/components/ota/ota_backend.h"

#include <memory>
#include <utility>
#include <string>

namespace esphome {
namespace ota_http {

static const uint8_t OTA_HTTP_STATE_OK = 10;
static const uint8_t OTA_HTTP_STATE_PROGRESS = 20;
static const uint8_t OTA_HTTP_STATE_SAFE_MODE = 30;
static const uint8_t OTA_HTTP_STATE_ABORT = 40;

#define OTA_HTTP_PREF_SAFE_MODE_HASH 99380598UL

static const char *const TAG = "ota_http";

struct otaHttpGlobalPrefType {
  int ota_http_state;
  char url[128];
  bool verify_ssl;
} PACKED;

class OtaHttpComponent : public Component {
 public:
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
  void set_url(std::string url) {
    // this->url_ = std::move(url);
    // TODO check size
    size_t length = std::min(url.length(), sizeof(pref_.url) - 1);
    strncpy(pref_.url, url.c_str(), length);
    pref_.url[length] = '\0';
    // this->secure_ = url.rfind("https:", 0) == 0;
    // this->secure_ = strncmp(pref_.url, "https:", 6) == 0;
    pref_obj_.save(&pref_);
  }
  void set_timeout(uint64_t timeout) { this->timeout_ = timeout; }
  void flash();
  void check_upgrade();
  virtual int http_init() { return -1; };
  virtual size_t http_read(uint8_t *buf, size_t len) { return 0; };
  virtual void http_end(){};

 protected:
  // std::string url_;
  bool secure_() { return strncmp(pref_.url, "https:", 6) == 0; };
  size_t body_length_ = 0;
  size_t bytes_read_ = 0;
  uint64_t timeout_{1000 * 60 * 10};            // must match CONF_TIMEOUT in __init__.py
  const uint16_t http_recv_buffer_ = 1000;      // the firwmware GET chunk size
  const uint16_t max_http_recv_buffer_ = 1024;  // internal max http buffer size must be > HTTP_RECV_BUFFER_ (TLS
                                                // overhead) and must be a power of two from 512 to 4096
  bool update_started_ = false;
  static const std::unique_ptr<ota::OTABackend> BACKEND;
  void cleanup_();
  otaHttpGlobalPrefType pref_ = {OTA_HTTP_STATE_OK, "", true};
  ESPPreferenceObject pref_obj_ =
      global_preferences->make_preference<otaHttpGlobalPrefType>(OTA_HTTP_PREF_SAFE_MODE_HASH, true);
};

template<typename... Ts> class OtaHttpFlashAction : public Action<Ts...> {
 public:
  OtaHttpFlashAction(OtaHttpComponent *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, url)
  TEMPLATABLE_VALUE(uint16_t, timeout)

  void play(Ts... x) override {
    this->parent_->set_url(this->url_.value(x...));
    if (this->timeout_.has_value()) {
      this->parent_->set_timeout(this->timeout_.value(x...));
    }
    this->parent_->flash();
  }

 protected:
  OtaHttpComponent *parent_;
};

}  // namespace ota_http
}  // namespace esphome
