#include "micronova_text_sensor.h"

namespace esphome::micronova {

void MicroNovaTextSensor::process_value_from_stove(int value_from_stove) {
  if (value_from_stove == -1) {
    this->publish_state("unknown");
    return;
  }

  switch (this->get_function()) {
    case MicroNovaFunctions::STOVE_FUNCTION_STOVE_STATE:
      this->publish_state(STOVE_STATES[value_from_stove]);
      break;
    default:
      break;
  }
}

}  // namespace esphome::micronova
