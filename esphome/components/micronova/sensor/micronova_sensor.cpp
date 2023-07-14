#include "micronova_sensor.h"

namespace esphome {
namespace micronova {

void MicroNovaSensor::read_value_from_stove() {
  int new_raw_value = -1;

  new_raw_value = this->micronova_->read_address(this->memory_location_, this->memory_address_);
  if (new_raw_value == -1) {
    this->publish_state(NAN);
    return;
  }

  float new_sensor_value = (float) new_raw_value;
  switch (this->get_function()) {
    case MicroNovaFunctions::STOVE_FUNCTION_ROOM_TEMPERATURE:
      new_sensor_value = (float) new_sensor_value / 2;
      break;
    case MicroNovaFunctions::STOVE_FUNCTION_THERMOSTAT_TEMPERATURE:
      this->micronova_->set_thermostat_temperature(new_sensor_value);
      break;
    case MicroNovaFunctions::STOVE_FUNCTION_FAN_SPEED:
      new_sensor_value = new_sensor_value == 0 ? 0 : (new_sensor_value * 10) + this->fan_speed_offset_;
      break;
    default:
      break;
  }
  this->publish_state(new_sensor_value);
}

}  // namespace micronova
}  // namespace esphome
