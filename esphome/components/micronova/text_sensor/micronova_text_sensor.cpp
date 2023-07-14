#include "micronova_text_sensor.h"

namespace esphome {
namespace micronova {

void MicroNovaTextSensor::publish_val(int new_raw_value) {
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
