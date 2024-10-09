#include "input_impedance_select.h"

namespace esphome {
namespace qn8027 {

void InputImpedanceSelect::control(const std::string &value) {
  this->publish_state(value);
  if (auto index = this->active_index()) {
    this->parent_->set_input_impedance((InputImpedance) *index);
  }
}

}  // namespace qn8027
}  // namespace esphome
