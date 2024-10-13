#include "alc_low_select.h"

namespace esphome {
namespace kt0803 {

void AlcLowSelect::control(const std::string &value) {
  this->publish_state(value);
  if (auto index = this->active_index()) {
    this->parent_->set_alc_low((AlcLow) *index);
  }
}

}  // namespace kt0803
}  // namespace esphome
