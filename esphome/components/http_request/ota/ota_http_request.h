#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/components/ota/ota_backend.h"

#include <memory>
#include <string>
#include <utility>

namespace esphome {
namespace http_request {

enum OtaHttpRequestState {
  OTA_HTTP_REQUEST_STATE_OK,
  OTA_HTTP_REQUEST_STATE_PROGRESS,
  OTA_HTTP_REQUEST_STATE_SAFE_MODE,
  OTA_HTTP_REQUEST_STATE_ABORT,
};

#define OTA_HTTP_PREF_SAFE_MODE_HASH 99380598UL
#ifndef CONFIG_MAX_URL_LENGTH
static const uint16_t CONFIG_MAX_URL_LENGTH = 128;
#endif

static const char *const TAG = "http_request.ota";
static const uint8_t MD5_SIZE = 32;

struct OtaHttpRequestGlobalPrefType {
  OtaHttpRequestState ota_http_request_state;
  char last_md5[MD5_SIZE + 1];
  char md5_url[CONFIG_MAX_URL_LENGTH];
  char url[CONFIG_MAX_URL_LENGTH];
} PACKED;

class OtaHttpRequestComponent : public ota::OTAComponent {
 public:
  OtaHttpRequestComponent();
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
  bool save_md5_url(const std::string &md5_url) { return this->save_url_(md5_url, this->pref_.md5_url); }
  bool save_url(const std::string &url) { return this->save_url_(url, this->pref_.url); }
  void set_force_update(bool force_update) { this->force_update_ = force_update; }
  bool set_url(char *url);
  void set_timeout(uint64_t timeout) { this->timeout_ = timeout; }
  bool check_status();
  void check_upgrade();
  void flash();
  bool http_get_md5();
  virtual void http_init(){};
  virtual int http_read(uint8_t *buf, size_t len) { return 0; };
  virtual void http_end(){};

 protected:
  void cleanup_(std::unique_ptr<ota::OTABackend> backend, uint8_t error_code);
  bool secure_() { return strncmp(this->url_, "https:", 6) == 0; };
  char *url_ = nullptr;
  char md5_expected_[MD5_SIZE];
  char safe_url_[CONFIG_MAX_URL_LENGTH];
  bool force_update_ = false;
  bool safe_mode_ = false;
  bool update_started_ = false;
  size_t body_length_ = 0;
  size_t bytes_read_ = 0;
  int status_ = -1;
  uint64_t timeout_;
  const uint16_t http_recv_buffer_ = 500;      // the firmware GET chunk size
  const uint16_t max_http_recv_buffer_ = 512;  // internal max http buffer size must be > HTTP_RECV_BUFFER_ (TLS
                                               // overhead) and must be a power of two from 512 to 4096
  OtaHttpRequestGlobalPrefType pref_ = {OTA_HTTP_REQUEST_STATE_OK, "None", "", ""};
  ESPPreferenceObject pref_obj_ =
      global_preferences->make_preference<OtaHttpRequestGlobalPrefType>(OTA_HTTP_PREF_SAFE_MODE_HASH, true);

 private:
  bool save_url_(const std::string &value, char *url);
  void set_safe_url_();
};

template<typename... Ts> class OtaHttpRequestComponentFlashAction : public Action<Ts...> {
 public:
  OtaHttpRequestComponentFlashAction(OtaHttpRequestComponent *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, md5_url)
  TEMPLATABLE_VALUE(std::string, url)
  TEMPLATABLE_VALUE(bool, force_update)
  TEMPLATABLE_VALUE(uint64_t, timeout)

  void play(Ts... x) override {
    if (this->parent_->save_md5_url(this->md5_url_.value(x...)) && this->parent_->save_url(this->url_.value(x...))) {
      if (this->timeout_.has_value()) {
        this->parent_->set_timeout(this->timeout_.value(x...));
      }
      this->parent_->set_force_update(this->force_update_.value(x...));
      this->parent_->flash();
      // Normaly never reached (device rebooted)
    }
  }

 protected:
  OtaHttpRequestComponent *parent_;
};

}  // namespace http_request
}  // namespace esphome
