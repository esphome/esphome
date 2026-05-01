#include "mpr121_binary_sensor.h"

namespace esphome {
namespace mpr121 {

void MPR121BinarySensor::setup() {
  uint8_t touch_threshold = this->touch_threshold_.value_or(this->parent_->get_touch_threshold());
  this->parent_->write_byte(MPR121_TOUCHTH_0 + 2 * this->channel_, touch_threshold);

  uint8_t release_threshold = this->release_threshold_.value_or(this->parent_->get_release_threshold());
  this->parent_->write_byte(MPR121_RELEASETH_0 + 2 * this->channel_, release_threshold);
}

void MPR121BinarySensor::process(uint16_t data) {
  bool new_state = data & (1 << this->channel_);
  this->publish_state(new_state);
}

}  // namespace mpr121
}  // namespace esphome
