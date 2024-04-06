#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/components/ota/ota_backend.h"

#include <memory>
#include <utility>
#include <string>
#include <regex>

namespace esphome {
namespace ota_http {

enum OtaHttpState {
  OTA_HTTP_STATE_OK,
  OTA_HTTP_STATE_PROGRESS,
  OTA_HTTP_STATE_SAFE_MODE,
  OTA_HTTP_STATE_ABORT,
};

#define OTA_HTTP_PREF_SAFE_MODE_HASH 99380598UL
#ifndef CONFIG_MAX_URL_LENGTH
static const uint16_t CONFIG_MAX_URL_LENGTH = 128;
#endif
#ifndef CONFIG_FORCE_UPDATE
static const bool CONFIG_FORCE_UPDATE = true;
#endif

static const char *const TAG = "ota_http";
static const uint8_t MD5_SIZE = 32;

struct OtaHttpGlobalPrefType {
  OtaHttpState ota_http_state;
  char last_md5[MD5_SIZE + 1];
  char md5_url[CONFIG_MAX_URL_LENGTH];
  char url[CONFIG_MAX_URL_LENGTH];
} PACKED;

class OtaHttpComponent : public Component {
 public:
  OtaHttpComponent();
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
  bool set_md5_url(const std::string &md5_url) { return this->set_url_(md5_url, this->pref_.md5_url); }
  bool set_url(const std::string &url) { return this->set_url_(url, this->pref_.url); }
  static std::string safe_url(const char* url) {
    std::regex urlPattern(R"(^(https?:\/\/)([^:]+):([^@]+)@)");
    return std::regex_replace(url, urlPattern, "$1*****:*****@");
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
  bool safe_mode_ = false;
  uint64_t timeout_;
  const uint16_t http_recv_buffer_ = 500;      // the firmware GET chunk size
  const uint16_t max_http_recv_buffer_ = 512;  // internal max http buffer size must be > HTTP_RECV_BUFFER_ (TLS
                                               // overhead) and must be a power of two from 512 to 4096
  bool update_started_ = false;
  static const std::unique_ptr<ota::OTABackend> BACKEND;
  void cleanup_();
  char md5_expected_[MD5_SIZE];
  OtaHttpGlobalPrefType pref_ = {OTA_HTTP_STATE_OK, "None", "", ""};
  ESPPreferenceObject pref_obj_ =
      global_preferences->make_preference<OtaHttpGlobalPrefType>(OTA_HTTP_PREF_SAFE_MODE_HASH, true);

 private:
  bool set_url_(const std::string &value, char *url);
};

template<typename... Ts> class OtaHttpFlashAction : public Action<Ts...> {
 public:
  OtaHttpFlashAction(OtaHttpComponent *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, md5_url)
  TEMPLATABLE_VALUE(std::string, url)
  TEMPLATABLE_VALUE(uint64_t, timeout)

  void play(Ts... x) override {
    if (this->parent_->set_md5_url(this->md5_url_.value(x...)) && this->parent_->set_url(this->url_.value(x...))) {
      if (this->timeout_.has_value()) {
        this->parent_->set_timeout(this->timeout_.value(x...));
      }
      this->parent_->flash();
      // Normaly never reached (device rebooted)
    }
  }

 protected:
  OtaHttpComponent *parent_;
};

}  // namespace ota_http
}  // namespace esphome
