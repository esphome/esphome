#include "restart_button.h"

namespace esphome::ld2450 {

void RestartButton::press_action() { this->parent_->restart_and_read_all_info(); }

}  // namespace esphome::ld2450
