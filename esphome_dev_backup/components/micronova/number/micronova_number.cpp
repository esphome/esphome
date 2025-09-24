#include "micronova_number.h"

namespace esphome {
namespace micronova {

void MicroNovaNumber::process_value_from_stove(int value_from_stove) {
  float new_sensor_value = 0;

  if (value_from_stove == -1) {
    this->publish_state(NAN);
    return;
  }

  switch (this->get_function()) {
    case MicroNovaFunctions::STOVE_FUNCTION_THERMOSTAT_TEMPERATURE:
      new_sensor_value = ((float) value_from_stove) * this->traits.get_step();
      break;
    case MicroNovaFunctions::STOVE_FUNCTION_POWER_LEVEL:
      new_sensor_value = (float) value_from_stove;
      break;
    default:
      break;
  }
  this->publish_state(new_sensor_value);
}

void MicroNovaNumber::control(float value) {
  uint8_t new_number = 0;

  switch (this->get_function()) {
    case MicroNovaFunctions::STOVE_FUNCTION_THERMOSTAT_TEMPERATURE:
      new_number = (uint8_t) (value / this->traits.get_step());
      break;
    case MicroNovaFunctions::STOVE_FUNCTION_POWER_LEVEL:
      new_number = (uint8_t) value;
      break;
    default:
      break;
  }
  this->micronova_->write_address(this->memory_write_location_, this->memory_address_, new_number);
  this->micronova_->update();
}

}  // namespace micronova
}  // namespace esphome
