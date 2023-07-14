#include "micronova_text_sensor.h"

namespace esphome {
namespace micronova {

void MicroNovaTextSensor::read_value_from_stove() {
  int new_raw_value = -1;

  new_raw_value = this->micronova_->read_address(this->memory_location_, this->memory_address_);
  if (new_raw_value == -1) {
    this->publish_state("unknown");
    return;
  }

  switch (this->get_function()) {
    case MicroNovaFunctions::STOVE_FUNCTION_STOVE_STATE:
      this->micronova_->set_current_stove_state(new_raw_value);
      this->publish_state(STOVE_STATES[new_raw_value]);
      // set the stove switch to on for any value but 0
      if (new_raw_value != 0 && this->micronova_->get_stove_switch() != nullptr &&
          !this->micronova_->get_stove_switch()->get_stove_switch_state()) {
        this->micronova_->get_stove_switch()->set_stove_switch_state(true);
      } else if (new_raw_value == 0 && this->micronova_->get_stove_switch() != nullptr &&
                 this->micronova_->get_stove_switch()->get_stove_switch_state()) {
        this->micronova_->get_stove_switch()->set_stove_switch_state(false);
      }
      break;
    default:
      break;
  }
}

}  // namespace micronova
}  // namespace esphome
