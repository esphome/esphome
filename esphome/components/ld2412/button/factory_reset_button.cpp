#include "factory_reset_button.h"

namespace esphome {
namespace ld2412 {

void FactoryResetButton::press_action() { this->parent_->factory_reset(); }

}  // namespace ld2412
}  // namespace esphome
