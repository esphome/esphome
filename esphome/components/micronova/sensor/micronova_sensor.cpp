#include "micronova_sensor.h"

namespace esphome {
namespace micronova {

void MicroNovaSensor::process_value_from_stove(int value_from_stove) {
  if (value_from_stove == -1) {
    this->publish_state(NAN);
    return;
  }

  float new_sensor_value = (float) value_from_stove;
  switch (this->get_function()) {
    case MicroNovaFunctions::STOVE_FUNCTION_ROOM_TEMPERATURE:
      new_sensor_value = new_sensor_value / 2;
      break;
    case MicroNovaFunctions::STOVE_FUNCTION_THERMOSTAT_TEMPERATURE:
      break;
    case MicroNovaFunctions::STOVE_FUNCTION_FAN_SPEED:
      new_sensor_value = new_sensor_value == 0 ? 0 : (new_sensor_value * 10) + this->fan_speed_offset_;
      break;
    case MicroNovaFunctions::STOVE_FUNCTION_WATER_TEMPERATURE:
      new_sensor_value = new_sensor_value / 2;
      break;
    case MicroNovaFunctions::STOVE_FUNCTION_WATER_PRESSURE:
      new_sensor_value = new_sensor_value / 10;
      break;
    default:
      break;
  }
  this->publish_state(new_sensor_value);
}

}  // namespace micronova
}  // namespace esphome
