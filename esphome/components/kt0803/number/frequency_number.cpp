#include "frequency_number.h"

namespace esphome {
namespace kt0803 {

void FrequencyNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_frequency(value);
}

}  // namespace kt0803
}  // namespace esphome
