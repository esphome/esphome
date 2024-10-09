#include "xtal_frequency_select.h"

namespace esphome {
namespace qn8027 {

void XtalFrequencySelect::control(const std::string &value) {
  this->publish_state(value);
  if (auto index = this->active_index()) {
    this->parent_->set_xtal_frequency((XtalFrequency) *index);
  }
}

}  // namespace qn8027
}  // namespace esphome
