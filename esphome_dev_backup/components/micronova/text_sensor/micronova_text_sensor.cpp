#include "micronova_text_sensor.h"

namespace esphome {
namespace micronova {

void MicroNovaTextSensor::process_value_from_stove(int value_from_stove) {
  if (value_from_stove == -1) {
    this->publish_state("unknown");
    return;
  }

  switch (this->get_function()) {
    case MicroNovaFunctions::STOVE_FUNCTION_STOVE_STATE:
      this->micronova_->set_current_stove_state(value_from_stove);
      this->publish_state(STOVE_STATES[value_from_stove]);
      // set the stove switch to on for any value but 0
      if (value_from_stove != 0 && this->micronova_->get_stove_switch() != nullptr &&
          !this->micronova_->get_stove_switch()->get_stove_state()) {
        this->micronova_->get_stove_switch()->set_stove_state(true);
      } else if (value_from_stove == 0 && this->micronova_->get_stove_switch() != nullptr &&
                 this->micronova_->get_stove_switch()->get_stove_state()) {
        this->micronova_->get_stove_switch()->set_stove_state(false);
      }
      break;
    default:
      break;
  }
}

}  // namespace micronova
}  // namespace esphome
