#include "silence_low_select.h"

namespace esphome {
namespace kt0803 {

void SilenceLowSelect::control(const std::string &value) {
  this->publish_state(value);
  if (auto index = this->active_index()) {
    this->parent_->set_silence_low((SilenceLow) *index);
  }
}

}  // namespace kt0803
}  // namespace esphome
