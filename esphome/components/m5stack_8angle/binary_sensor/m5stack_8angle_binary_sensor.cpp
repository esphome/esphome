#include "m5stack_8angle_binary_sensor.h"

namespace esphome::m5stack_8angle {

void M5Stack8AngleSwitchBinarySensor::update() {
  int8_t out = this->parent_->read_switch();
  if (!this->status_comm_result(out != -1, LOG_STR("Could not read binary sensor state from M5Stack 8Angle.")))
    return;
  this->publish_state(out != 0);
}

}  // namespace esphome::m5stack_8angle
