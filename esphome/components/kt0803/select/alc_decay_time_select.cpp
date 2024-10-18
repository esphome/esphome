#include "alc_decay_time_select.h"

namespace esphome {
namespace kt0803 {

void AlcDecayTimeSelect::control(const std::string &value) {
  this->publish_state(value);
  if (auto index = this->active_index()) {
    this->parent_->set_alc_decay_time((AlcTime) *index);
  }
}

}  // namespace kt0803
}  // namespace esphome
