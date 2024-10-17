#include "silence_high_select.h"

namespace esphome {
namespace kt0803 {

void SilenceHighSelect::control(const std::string &value) {
  this->publish_state(value);
  if (auto index = this->active_index()) {
    this->parent_->set_silence_high((SilenceHigh) *index);
  }
}

}  // namespace kt0803
}  // namespace esphome
