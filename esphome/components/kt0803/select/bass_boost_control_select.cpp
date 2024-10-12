#include "bass_boost_control_select.h"

namespace esphome {
namespace kt0803 {

void BassBoostControlSelect::control(const std::string &value) {
  this->publish_state(value);
  if (auto index = this->active_index()) {
    this->parent_->set_bass_boost_control((BassBoostControl) *index);
  }
}

}  // namespace kt0803
}  // namespace esphome
