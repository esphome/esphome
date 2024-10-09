#include "input_gain_number.h"

namespace esphome {
namespace qn8027 {

void InputGainNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_input_gain((uint8_t) value);
}

}  // namespace qn8027
}  // namespace esphome
