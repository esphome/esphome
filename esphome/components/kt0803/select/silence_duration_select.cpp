#include "silence_duration_select.h"

namespace esphome {
namespace kt0803 {

void SilenceDurationSelect::control(const std::string &value) {
  this->publish_state(value);
  if (auto index = this->active_index()) {
    this->parent_->set_silence_duration((SilenceLowAndHighLevelDurationTime) *index);
  }
}

}  // namespace kt0803
}  // namespace esphome
