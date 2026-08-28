#include "ld6002b_select.h"

namespace esphome::ld6002b {

void LD6002BSelect::control(size_t index) {
  this->publish_state(index);
  this->parent_->set_select_value(this->type_, index);
}

}  // namespace esphome::ld6002b
