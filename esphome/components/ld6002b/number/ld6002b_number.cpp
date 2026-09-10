#include "ld6002b_number.h"

namespace esphome::ld6002b {

void LD6002BNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_number_value(this->type_, value);
}

}  // namespace esphome::ld6002b
