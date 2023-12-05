#include "reset_button.h"

namespace esphome {
namespace mr24hpc1 {

void ResetButton::press_action() {
    this->parent_->set_reset();
}

}  // namespace mr24hpc1
}  // namespace esphome