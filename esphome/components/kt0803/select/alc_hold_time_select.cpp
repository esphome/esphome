#include "alc_hold_time_select.h"

namespace esphome {
namespace kt0803 {

void AlcHoldTimeSelect::control(const std::string &value) {
  this->publish_state(value);
  if (auto index = this->active_index()) {
    this->parent_->set_alc_hold_time((AlcHoldTime) *index);
  }
}

}  // namespace kt0803
}  // namespace esphome
