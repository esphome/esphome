#include "switch_mode_select.h"

namespace esphome {
namespace kt0803 {

void SwitchModeSelect::control(const std::string &value) {
  this->publish_state(value);
  if (auto index = this->active_index()) {
    this->parent_->set_switch_mode((SwitchMode) *index);
  }
}

}  // namespace kt0803
}  // namespace esphome
