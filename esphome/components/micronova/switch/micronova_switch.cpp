#include "micronova_switch.h"

namespace esphome::micronova {

void MicroNovaSwitch::write_state(bool state) {
  auto data = state ? this->memory_data_on_ : this->memory_data_off_;
  this->micronova_->write_address(this->memory_location_, this->memory_address_, data);
  this->publish_state(state);
}

void MicroNovaSwitch::process_value_from_stove(int value_from_stove) {
  if (value_from_stove == -1) {
    ESP_LOGE(TAG, "Error reading stove state");
    return;
  }

  // set the stove switch to on for any value but 0
  bool state = value_from_stove != 0;
  this->publish_state(state);
}

}  // namespace esphome::micronova
