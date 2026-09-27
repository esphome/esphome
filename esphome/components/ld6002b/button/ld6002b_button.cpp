#include "ld6002b_button.h"

namespace esphome::ld6002b {

void LD6002BButton::press_action() { this->parent_->press_button(this->type_); }

}  // namespace esphome::ld6002b
