#include "micronova_sensor.h"

namespace esphome::micronova {

void MicroNovaSensor::process_value_from_stove(int value_from_stove) {
  if (value_from_stove == -1) {
    this->publish_state(NAN);
    return;
  }

  float new_sensor_value = static_cast<float>(value_from_stove);

  // Fan speed has special calculation: value * 10 + offset (when non-zero)
  if (this->is_fan_speed_) {
    new_sensor_value = value_from_stove == 0 ? 0.0f : (new_sensor_value * 10) + this->fan_speed_offset_;
  } else if (this->divisor_ > 1) {
    new_sensor_value = new_sensor_value / this->divisor_;
  }

  this->publish_state(new_sensor_value);
}

}  // namespace esphome::micronova
