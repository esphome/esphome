#include "custom_mode_end_button.h"

namespace esphome {
namespace seeed_mr24hpc1 {

void CustomSetEndButton::press_action() { this->parent_->set_custom_end_mode(); }

}  // namespace seeed_mr24hpc1
}  // namespace esphome
