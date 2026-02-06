#include "micronova_switch.h"

namespace esphome::micronova {

void MicroNovaSwitch::write_state(bool state) {
  if (state) {
    // Only send power-on when current state is Off
    if (this->raw_state_ == 0) {
      this->micronova_->write_address(this->memory_location_, this->memory_address_, this->memory_data_on_);
      this->publish_state(true);
    } else {
      ESP_LOGW(TAG, "Unable to turn stove on, invalid state: %d", this->raw_state_);
    }
  } else {
    // don't send power-off when status is Off or Final cleaning
    if (this->raw_state_ != 0 && this->raw_state_ != 6) {
      this->micronova_->write_address(this->memory_location_, this->memory_address_, this->memory_data_off_);
      this->publish_state(false);
    } else {
      ESP_LOGW(TAG, "Unable to turn stove off, invalid state: %d", this->raw_state_);
    }
  }
  this->set_needs_update(true);
}

void MicroNovaSwitch::process_value_from_stove(int value_from_stove) {
  this->raw_state_ = value_from_stove;
  if (value_from_stove == -1) {
    ESP_LOGE(TAG, "Error reading stove state");
    return;
  }

  // set the stove switch to on for any value but 0
  bool state = value_from_stove != 0;
  this->publish_state(state);
}

}  // namespace esphome::micronova
