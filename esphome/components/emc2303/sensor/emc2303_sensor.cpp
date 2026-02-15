#include "emc2303_sensor.h"

namespace esphome::emc2303 {

static const char *const TAG = "emc2303.sensor";

void Emc2303Sensor::update() {
  if (this->speed_sensor_ != nullptr) {
    float speed = this->parent_->get_speed(this->fan_);
    this->speed_sensor_->publish_state(speed);
  }
}

}  // namespace esphome::emc2303
