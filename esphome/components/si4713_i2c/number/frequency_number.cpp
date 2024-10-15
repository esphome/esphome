#include "frequency_number.h"

namespace esphome {
namespace si4713 {

void FrequencyNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_frequency(value);
}

}  // namespace si4713
}  // namespace esphome
