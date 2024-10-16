#include "acomp_threshold_number.h"

namespace esphome {
namespace si4713 {

void AcompThresholdNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_acomp_threshold(value);
}

}  // namespace si4713
}  // namespace esphome