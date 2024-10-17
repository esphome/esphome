#include "alc_attack_time_select.h"

namespace esphome {
namespace kt0803 {

void AlcAttackTimeSelect::control(const std::string &value) {
  this->publish_state(value);
  if (auto index = this->active_index()) {
    this->parent_->set_alc_attack_time((AlcTime) *index);
  }
}

}  // namespace kt0803
}  // namespace esphome
