#include "factory_reset_button.h"

namespace esphome::ld2450 {

void FactoryResetButton::press_action() { this->parent_->factory_reset(); }

}  // namespace esphome::ld2450
