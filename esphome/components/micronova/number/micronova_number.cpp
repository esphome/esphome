#include "micronova_number.h"

namespace esphome::micronova {

void MicroNovaNumber::process_value_from_stove(int value_from_stove) {
  if (value_from_stove == -1) {
    this->publish_state(NAN);
    return;
  }

  float new_value = static_cast<float>(value_from_stove);
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
  this->micronova_->write_address(this->memory_location_, this->memory_address_, new_number);
  this->micronova_->request_update_listeners();
}

}  // namespace esphome::micronova
