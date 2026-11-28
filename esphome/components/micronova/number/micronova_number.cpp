#include "micronova_number.h"

namespace esphome::micronova {

ESPHOME_LOG_TAG(TAG, "micronova.number");

void MicroNovaNumber::dump_config() {
  LOG_NUMBER("", "Micronova number", this);
  this->dump_base_config();
  if (this->value_offset_ != 0) {
    ESP_LOGCONFIG(TAG, "  Value offset: %u", this->value_offset_);
  }
  if (this->has_mirror_write_) {
    ESP_LOGCONFIG(TAG, "  Mirror write location: %02X, address: %02X", this->mirror_location_, this->mirror_address_);
  }
}

void MicroNovaNumber::process_value_from_stove(int value_from_stove) {
  if (value_from_stove == -1) {
    this->publish_state(NAN);
    return;
  }

  if (value_from_stove < this->value_offset_) {
    ESP_LOGW(TAG, "Raw value 0x%02X is below value offset %u, ignoring", value_from_stove, this->value_offset_);
    return;
  }

  float new_value = static_cast<float>(value_from_stove - this->value_offset_);
  if (this->use_step_scaling_) {
    new_value *= this->traits.get_step();
  }
  this->publish_state(new_value);
}

void MicroNovaNumber::control(float value) {
  uint8_t new_number;
  if (this->use_step_scaling_) {
    new_number = static_cast<uint8_t>(value / this->traits.get_step());
  } else {
    new_number = static_cast<uint8_t>(value);
  }
  new_number += this->value_offset_;
  this->micronova_->queue_write_command(this->memory_location_, this->memory_address_, new_number);
  if (this->has_mirror_write_) {
    this->micronova_->queue_write_command(this->mirror_location_, this->mirror_address_, new_number);
  }
}

}  // namespace esphome::micronova
