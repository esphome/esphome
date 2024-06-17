#include "m5stack_8angle_sensor.h"

namespace esphome {
namespace m5stack_8angle {

void M5Stack8AngleKnobSensor::update() {
  if (this->parent_ != nullptr) {
    int32_t raw_pos = this->parent_->read_knob_pos_raw(this->channel_, this->bits_);
    if (raw_pos == -1) {
      this->status_set_warning("Could not read knob position from M5Stack 8Angle.");
      return;
    }
    if (this->raw_) {
      this->publish_state(raw_pos);
    } else {
      float knob_pos = (float) raw_pos / ((1 << this->bits_) - 1);
      this->publish_state(knob_pos);
    }
    this->status_clear_warning();
  };
}

}  // namespace m5stack_8angle
}  // namespace esphome
