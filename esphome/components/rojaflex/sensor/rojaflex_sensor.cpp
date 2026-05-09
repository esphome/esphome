#include "rojaflex_sensor.h"

namespace esphome::rojaflex {

void RojaflexSensor::update() {
  if (this->parent_ == nullptr) {
    return;
  }
  switch (this->type_) {
    case RojaflexSensorType::MOTOR_PCT: {
      const int pct = this->parent_->get_motor_pct(this->channel_);
      if (pct >= 0) {
        this->publish_state(pct);
      }
      break;
    }
  }
}

}  // namespace esphome::rojaflex
