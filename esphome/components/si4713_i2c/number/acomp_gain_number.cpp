#include "acomp_gain_number.h"

namespace esphome {
namespace si4713 {

void AcompGainNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_acomp_gain(value);
}

}  // namespace si4713
}  // namespace esphome