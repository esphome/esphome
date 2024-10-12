#include "alc_gain_number.h"

namespace esphome {
namespace kt0803 {

void AlcGainNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_alc_gain(value);
}

}  // namespace kt0803
}  // namespace esphome
