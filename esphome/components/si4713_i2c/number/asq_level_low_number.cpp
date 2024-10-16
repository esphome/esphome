#include "asq_level_low_number.h"

namespace esphome {
namespace si4713 {

void AsqLevelLowNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_asq_level_low(value);
}

}  // namespace si4713
}  // namespace esphome