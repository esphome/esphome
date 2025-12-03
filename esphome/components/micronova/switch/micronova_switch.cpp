#include "micronova_switch.h"

namespace esphome {
namespace micronova {

void MicroNovaSwitch::write_state(bool state) {
  switch (this->get_function()) {
    case MicroNovaFunctions::STOVE_FUNCTION_SWITCH: {
      auto data = state ? this->memory_data_on_ : this->memory_data_off_;
      this->micronova_->queue_write_command(this->memory_location_, this->memory_address_, data);
      this->publish_state(state);
      break;
    }
    default:
      break;
  }
}

void MicroNovaSwitch::process_value_from_stove(int value_from_stove) {
  if (value_from_stove == -1) {
    ESP_LOGE(TAG, "Error reading stove state");
    return;
  }

  switch (this->get_function()) {
    case MicroNovaFunctions::STOVE_FUNCTION_SWITCH: {
      // set the stove switch to on for any value but 0
      bool state = value_from_stove != 0;
      this->publish_state(state);
      break;
    }
    default:
      break;
  }
}

void MicroNovaSwitch::request_value_from_stove() {
  this->micronova_->queue_read_request(this->memory_location_-0x80, this->memory_address_);
}

}  // namespace micronova
}  // namespace esphome
