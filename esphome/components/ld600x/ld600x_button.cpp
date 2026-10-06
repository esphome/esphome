#include "ld600x_button.h"

#ifdef USE_BUTTON

namespace esphome::ld600x {

void LD600XButton::press_action() { this->parent_->press_button(this->kind_); }

}  // namespace esphome::ld600x

#endif  // USE_BUTTON
