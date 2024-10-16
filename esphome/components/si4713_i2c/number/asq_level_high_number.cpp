#include "asq_level_high_number.h"

namespace esphome {
namespace si4713 {

void AsqLevelHighNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_asq_level_high(value);
}

}  // namespace si4713
}  // namespace esphome