#include "tuner_power_number.h"

namespace esphome {
namespace si4713 {

void TunerPowerNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_tuner_power(value);
}

}  // namespace si4713
}  // namespace esphome