#include "quiet_mode.h"

namespace esphome {
namespace haier {

void QuietModeSwitch::write_state(bool state) {
  if (this->parent_->get_quiet_mode_state() != state) {
    this->parent_->set_quiet_mode_state(state);
  }
  this->publish_state(state);
}

}  // namespace haier
}  // namespace esphome
