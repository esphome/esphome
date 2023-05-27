#include "reset_button.h"

namespace esphome {
namespace ld2410 {

void ResetButton::press_action() { this->parent_->factory_reset(); }

}  // namespace ld2410
}  // namespace esphome
