#include "micronova_number.h"

namespace esphome::micronova {

static const char *const TAG = "micronova.number";

void MicroNovaNumber::dump_config() {
  LOG_NUMBER("", "Micronova number", this);
  this->dump_base_config();
}

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
  this->micronova_->queue_write_command(this->memory_location_, this->memory_address_, new_number);
}

}  // namespace esphome::micronova
