#include "factory_reset_button.h"

namespace esphome {
namespace ld2450 {

void FactoryResetButton::press_action() { this->parent_->factory_reset(); }

}  // namespace ld2450
}  // namespace esphome
