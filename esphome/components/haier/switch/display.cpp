#include "display.h"

namespace esphome {
namespace haier {

void DisplaySwitch::write_state(bool state) {
  if (this->parent_->get_display_state() != state) {
    this->parent_->set_display_state(state);
  }
  this->publish_state(state);
}

}  // namespace haier
}  // namespace esphome
