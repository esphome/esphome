#include "reset_button.h"

namespace esphome::tfluna {

void ResetButton::press_action() { this->parent_->factory_reset(); }

}  // namespace esphome::tfluna
