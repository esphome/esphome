#include "emc230x_sensor.h"

namespace esphome::emc230x {

static const char *const TAG = "emc230x.sensor";

void Emc230xSensor::update() {
  if (this->speed_sensor_ != nullptr) {
    float speed = this->parent_->get_speed(this->fan_);
    this->speed_sensor_->publish_state(speed);
  }
}

}  // namespace esphome::emc230x
