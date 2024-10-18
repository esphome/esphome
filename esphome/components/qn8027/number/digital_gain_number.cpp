#include "digital_gain_number.h"

namespace esphome {
namespace qn8027 {

void DigitalGainNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_digital_gain((uint8_t) lround(value));
}

}  // namespace qn8027
}  // namespace esphome
