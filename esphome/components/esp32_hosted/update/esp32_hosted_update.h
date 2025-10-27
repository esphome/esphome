#pragma once

#include "esphome/core/component.h"
#include "esphome/components/update/update_entity.h"

namespace esphome {
namespace esp32_hosted {

class Esp32HostedUpdate : public update::UpdateEntity, public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void perform(bool force) override;
  void check() override {}

  void set_firmware_data(const uint8_t *data) { this->firmware_data_ = data; }
  void set_firmware_size(size_t size) { this->firmware_size_ = size; }

 protected:
  const uint8_t *firmware_data_{nullptr};
  size_t firmware_size_{0};
};

}  // namespace esp32_hosted
}  // namespace esphome
