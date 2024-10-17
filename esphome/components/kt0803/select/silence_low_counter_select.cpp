#include "silence_low_counter_select.h"

namespace esphome {
namespace kt0803 {

void SilenceLowCounterSelect::control(const std::string &value) {
  this->publish_state(value);
  if (auto index = this->active_index()) {
    this->parent_->set_silence_low_counter((SilenceLowLevelCounter) *index);
  }
}

}  // namespace kt0803
}  // namespace esphome
