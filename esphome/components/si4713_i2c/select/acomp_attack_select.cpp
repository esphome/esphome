#include "acomp_attack_select.h"

namespace esphome {
namespace si4713 {

void AcompAttackSelect::control(const std::string &value) {
  this->publish_state(value);
  if (auto index = this->active_index()) {
    this->parent_->set_acomp_attack((AcompAttack) *index);
  }
}

}  // namespace si4713
}  // namespace esphome