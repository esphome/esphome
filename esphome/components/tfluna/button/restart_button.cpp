#include "restart_button.h"

namespace esphome::tfluna {

void RestartButton::press_action() { this->parent_->restart(); }

}  // namespace esphome::tfluna
