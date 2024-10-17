#include "xtal_current_number.h"

namespace esphome {
namespace qn8027 {

void XtalCurrentNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_xtal_current(value);
}

}  // namespace qn8027
}  // namespace esphome
