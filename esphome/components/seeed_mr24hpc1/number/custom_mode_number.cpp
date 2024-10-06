#include "custom_mode_number.h"

namespace esphome {
namespace seeed_mr24hpc1 {

void CustomModeNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_custom_mode(value);
}

}  // namespace seeed_mr24hpc1
}  // namespace esphome
