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

enum OtaHttpState {
  OTA_HTTP_STATE_OK,
  OTA_HTTP_STATE_PROGRESS,
  OTA_HTTP_STATE_SAFE_MODE,
  OTA_HTTP_STATE_ABORT,
};

#define OTA_HTTP_PREF_SAFE_MODE_HASH 99380598UL

static const char *const TAG = "ota_http";
static const uint8_t MD5_SIZE = 32;

struct OtaHttpGlobalPrefType {
  OtaHttpState ota_http_state;
  char md5_url[256];
  char url[256];
} PACKED;

class OtaHttpComponent : public Component {
 public:
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
  void set_md5_url(const std::string &md5_url) {
    size_t length = std::min(md5_url.length(), static_cast<size_t>(sizeof(this->pref_.md5_url) - 1));
    strncpy(this->pref_.md5_url, md5_url.c_str(), length);
    this->pref_.md5_url[length] = 0;  // null terminator
    this->pref_obj_.save(&this->pref_);
  }
  void set_url(const std::string &url) {
    size_t length = std::min(url.length(), static_cast<size_t>(sizeof(this->pref_.url) - 1));
    strncpy(this->pref_.url, url.c_str(), length);
    this->pref_.url[length] = 0;  // null terminator
    this->pref_obj_.save(&this->pref_);
  }
  void set_timeout(uint64_t timeout) { this->timeout_ = timeout; }
  void flash();
  void check_upgrade();
  bool http_get_md5();
  virtual int http_init(char *url) { return -1; };
  virtual int http_read(uint8_t *buf, size_t len) { return 0; };
  virtual void http_end(){};

 protected:
  bool secure_() { return strncmp(this->pref_.url, "https:", 6) == 0; };
  size_t body_length_ = 0;
  size_t bytes_read_ = 0;
  uint64_t timeout_;
  const uint16_t http_recv_buffer_ = 500;       // the firmware GET chunk size
  const uint16_t max_http_recv_buffer_ = 512;   // internal max http buffer size must be > HTTP_RECV_BUFFER_ (TLS
                                                // overhead) and must be a power of two from 512 to 4096
  bool update_started_ = false;
  static const std::unique_ptr<ota::OTABackend> BACKEND;
  void cleanup_();
  char md5_expected_[MD5_SIZE];
  OtaHttpGlobalPrefType pref_ = {OTA_HTTP_STATE_OK, "", ""};
  ESPPreferenceObject pref_obj_ =
      global_preferences->make_preference<OtaHttpGlobalPrefType>(OTA_HTTP_PREF_SAFE_MODE_HASH, true);
};

template<typename... Ts> class OtaHttpFlashAction : public Action<Ts...> {
 public:
  OtaHttpFlashAction(OtaHttpComponent *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, md5_url)
  TEMPLATABLE_VALUE(std::string, url)
  TEMPLATABLE_VALUE(uint64_t, timeout)

  void play(Ts... x) override {
    this->parent_->set_md5_url(this->md5_url_.value(x...));
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
