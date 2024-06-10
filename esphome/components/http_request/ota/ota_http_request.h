#pragma once

#include "esphome/components/ota/ota_backend.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

#include <memory>
#include <string>
#include <utility>

#include "../http_request.h"

namespace esphome {
namespace http_request {

static const char *const TAG = "http_request.ota";
static const uint8_t MD5_SIZE = 32;

enum OtaHttpRequestError : uint8_t {
  OTA_MD5_INVALID = 0x10,
  OTA_BAD_URL = 0x11,
  OTA_CONNECTION_ERROR = 0x12,
};

class OtaHttpRequestComponent : public ota::OTAComponent, public Parented<HttpRequestComponent> {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void set_md5_url(const std::string &md5_url);
  void set_md5(const std::string &md5) { this->md5_expected_ = md5; }
  void set_password(const std::string &password) { this->password_ = password; }
  void set_url(const std::string &url);
  void set_username(const std::string &username) { this->username_ = username; }

  std::string md5_computed() { return this->md5_computed_; }
  std::string md5_expected() { return this->md5_expected_; }

  void flash();

 protected:
  void cleanup_(std::unique_ptr<ota::OTABackend> backend, const std::shared_ptr<HttpContainer> &container);
  uint8_t do_ota_();
  std::string get_url_with_auth_(const std::string &url);
  bool http_get_md5_();
  bool validate_url_(const std::string &url);

  std::string md5_computed_{};
  std::string md5_expected_{};
  std::string md5_url_{};
  std::string password_{};
  std::string username_{};
  std::string url_{};
  int status_ = -1;
  bool update_started_ = false;
  static const uint16_t HTTP_RECV_BUFFER = 256;  // the firmware GET chunk size
};

}  // namespace http_request
}  // namespace esphome
