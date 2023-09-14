#include "micronova_number.h"

namespace esphome {
namespace micronova {

void MicroNovaNumber::process_value_from_stove(int value_from_stove) {
  if (value_from_stove == -1) {
    this->publish_state(NAN);
    return;
  }

  float new_sensor_value = (float) value_from_stove;
  switch (this->get_function()) {
    case MicroNovaFunctions::STOVE_FUNCTION_THERMOSTAT_TEMPERATURE:
      this->micronova_->set_thermostat_temperature(new_sensor_value);
      break;
    default:
      break;
  }
  this->publish_state(new_sensor_value);
}

void MicroNovaNumber::control(float value) {
  uint8_t new_temp = 0;

  switch (this->get_function()) {
    case MicroNovaFunctions::STOVE_FUNCTION_THERMOSTAT_TEMPERATURE:
      new_temp = (uint8_t) value;
      this->micronova_->write_address(this->memory_write_location_, this->memory_address_, new_temp);
      this->micronova_->update();
      break;
    default:
      break;
  }
}

}  // namespace micronova
}  // namespace esphome
