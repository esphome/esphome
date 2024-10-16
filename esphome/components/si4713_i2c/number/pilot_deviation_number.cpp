#include "pilot_deviation_number.h"

namespace esphome {
namespace si4713 {

void PilotDeviationNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_pilot_deviation(value);
}

}  // namespace si4713
}  // namespace esphome