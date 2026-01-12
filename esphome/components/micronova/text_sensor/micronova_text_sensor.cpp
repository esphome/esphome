#include "micronova_text_sensor.h"

namespace esphome::micronova {

void MicroNovaTextSensor::process_value_from_stove(int value_from_stove) {
  if (value_from_stove == -1) {
    this->publish_state("unknown");
    return;
  }

  this->publish_state(STOVE_STATES[value_from_stove]);
}

}  // namespace esphome::micronova
