#pragma once

#if defined(USE_ESP32_VARIANT_ESP32H2) || defined(USE_ESP32_VARIANT_ESP32P4)

#include "esphome/core/component.h"
#include "esphome/components/update/update_entity.h"
#include <array>
#include <string>

#ifdef USE_ESP32_HOSTED_HTTP_UPDATE
#include "esphome/components/http_request/http_request.h"
#endif

namespace esphome::esp32_hosted {

class Esp32HostedUpdate : public update::UpdateEntity, public PollingComponent {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void update() override { this->check(); }  // PollingComponent - delegates to check()
  void perform(bool force) override;
  void check() override;

#ifdef USE_ESP32_HOSTED_HTTP_UPDATE
  // HTTP mode setters
  void set_source_url(const std::string &url) { this->source_url_ = url; }
  void set_http_request_parent(http_request::HttpRequestComponent *parent) { this->http_request_parent_ = parent; }
#else
  // Embedded mode setters
  void set_firmware_data(const uint8_t *data) { this->firmware_data_ = data; }
  void set_firmware_size(size_t size) { this->firmware_size_ = size; }
  void set_firmware_sha256(const std::array<uint8_t, 32> &sha256) { this->firmware_sha256_ = sha256; }
#endif

 protected:
#ifdef USE_ESP32_HOSTED_HTTP_UPDATE
  // HTTP mode members
  http_request::HttpRequestComponent *http_request_parent_{nullptr};
  std::string source_url_;
  std::string firmware_url_;

  // HTTP mode helpers
  bool fetch_manifest_();
  bool stream_firmware_to_coprocessor_();
#else
  // Embedded mode members
  const uint8_t *firmware_data_{nullptr};
  size_t firmware_size_{0};

  // Embedded mode helper
  bool write_embedded_firmware_to_coprocessor_();
#endif

  std::array<uint8_t, 32> firmware_sha256_{};
};

}  // namespace esphome::esp32_hosted

#endif
