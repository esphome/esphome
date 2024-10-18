#include "alc_high_select.h"

namespace esphome {
namespace kt0803 {

void AlcHighSelect::control(const std::string &value) {
  this->publish_state(value);
  if (auto index = this->active_index()) {
    this->parent_->set_alc_high((AlcHigh) *index);
  }
}

}  // namespace kt0803
}  // namespace esphome
