#include "frequency_number.h"

namespace esphome {
namespace qn8027 {

void FrequencyNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_frequency(value);
}

}  // namespace qn8027
}  // namespace esphome
