#include "silence_high_counter_select.h"

namespace esphome {
namespace kt0803 {

void SilenceHighCounterSelect::control(const std::string &value) {
  this->publish_state(value);
  if (auto index = this->active_index()) {
    this->parent_->set_silence_high_counter((SilenceHighLevelCounter) *index);
  }
}

}  // namespace kt0803
}  // namespace esphome
