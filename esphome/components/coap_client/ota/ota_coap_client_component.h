#pragma once

#include "esphome/components/md5/md5.h"
#include "esphome/components/ota/ota_backend.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

#include <memory>
#include <string>
#include <utility>

#include "../coap_client_component.h"

namespace esphome::coap {

static const uint8_t MD5_SIZE = 32;

enum OtaCoapClientComponentError : uint8_t {
  OTA_MD5_INVALID = 0x10,
  OTA_BAD_URL = 0x11,
  OTA_CONNECTION_ERROR = 0x12,
  OTA_ABORT = 0x13,
  OTA_SERVER_FAIL = 0x14,
};

class OtaCoapClientComponent : public ota::OTAComponent, public Parented<CoapClientComponent> {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void set_md5_url(const std::string &md5_url);
  void set_md5(const std::string &md5) { this->md5_ = md5; }
  void set_url(const std::string &url);
  std::string md5_computed() { return this->md5_computed_; }
  std::string md5_expected() { return this->md5_expected_; }
  void append_image(const uint8_t *block, uint16_t length, uint32_t offset);
  void flash();
  void abort();

  static void md5_callback(uint16_t response_code, const unsigned char *data, size_t len, size_t offset, size_t total,
                           void *context);
  static void image_callback(uint16_t response_code, const unsigned char *data, size_t len, size_t offset, size_t total,
                             void *context);

  void append_md5_expected(const unsigned char *data, size_t len) {
    this->md5_expected_.append(reinterpret_cast<const char *>(data), len);
  }
  void set_md5_url_ready(bool ready) { this->md5_url_ready_ = ready; }
  bool is_md5_url_ready() { return this->md5_url_ready_; }
  bool is_image_download_abort() { return this->image_download_abort_; }
  void set_image_download_ready(bool ready) { this->image_download_ready_ = ready; }
  bool is_image_download_ready() { return this->image_download_ready_; }

 protected:
  uint8_t do_ota_();
  bool get_md5_expected_();
  bool validate_url_(const std::string &url);

  bool md5_url_ready_{false};
  std::string md5_computed_{};
  std::string md5_{};
  std::string md5_expected_{};
  std::string md5_url_{};
  std::string url_{};
  bool image_download_started_{false};
  bool image_download_abort_{false};
  bool image_download_ready_{false};
  std::unique_ptr<ota::OTABackend> backend_;
  md5::MD5Digest md5_receive_;
};

}  // namespace esphome::coap
