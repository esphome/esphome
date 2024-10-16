#include "pilot_frequency_number.h"

namespace esphome {
namespace si4713 {

void PilotFrequencyNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_pilot_frequency(value);
}

}  // namespace si4713
}  // namespace esphome