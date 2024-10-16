#include "digital_mode_select.h"

namespace esphome {
namespace si4713 {

void DigitalModeSelect::control(const std::string &value) {
  this->publish_state(value);
  if (auto index = this->active_index()) {
    this->parent_->set_digital_mode((DigitalMode) *index);
  }
}

}  // namespace si4713
}  // namespace esphome