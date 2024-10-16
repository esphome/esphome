#include "analog_level_number.h"

namespace esphome {
namespace si4713 {

void AnalogLevelNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_analog_level(value);
}

}  // namespace si4713
}  // namespace esphome