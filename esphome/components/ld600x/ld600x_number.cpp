#include "ld600x_number.h"

#ifdef USE_NUMBER

namespace esphome::ld600x {

void LD600XNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_number_value(this->kind_, value);
}

}  // namespace esphome::ld600x

#endif  // USE_NUMBER
