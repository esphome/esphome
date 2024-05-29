#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/components/ota/ota_backend.h"

#include <memory>
#include <string>
#include <utility>

namespace esphome {
namespace http_request {

static const char *const TAG = "http_request.ota";
static const uint8_t MD5_SIZE = 32;

enum OtaHttpRequestError : uint8_t {
  OTA_MD5_INVALID = 0x10,
  OTA_BAD_URL = 0x11,
  OTA_CONNECTION_ERROR = 0x12,
};

class OtaHttpRequestComponent : public ota::OTAComponent {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void set_md5_url(const std::string &md5_url);
  void set_md5(const std::string &md5) { this->md5_expected_ = md5; }
  void set_password(const std::string &password) { this->password_ = password; }
  void set_timeout(const uint64_t timeout) { this->timeout_ = timeout; }
  void set_url(const std::string &url);
  void set_username(const std::string &username) { this->username_ = username; }

  std::string md5_computed() { return this->md5_computed_; }
  std::string md5_expected() { return this->md5_expected_; }

  bool check_status();

  void flash();

  virtual void http_init(const std::string &url){};
  virtual int http_read(uint8_t *buf, size_t len) { return 0; };
  virtual void http_end(){};

 protected:
  void cleanup_(std::unique_ptr<ota::OTABackend> backend);
  uint8_t do_ota_();
  std::string get_url_with_auth_(const std::string &url);
  bool http_get_md5_();
  bool secure_() { return this->url_.find("https:") != std::string::npos; };
  bool validate_url_(const std::string &url);

  std::string md5_computed_{};
  std::string md5_expected_{};
  std::string md5_url_{};
  std::string password_{};
  std::string username_{};
  std::string url_{};
  size_t body_length_ = 0;
  size_t bytes_read_ = 0;
  int status_ = -1;
  uint64_t timeout_ = 0;
  bool update_started_ = false;
  const uint16_t http_recv_buffer_ = 256;      // the firmware GET chunk size
  const uint16_t max_http_recv_buffer_ = 512;  // internal max http buffer size must be > HTTP_RECV_BUFFER_ (TLS
                                               // overhead) and must be a power of two from 512 to 4096
};

}  // namespace http_request
}  // namespace esphome
