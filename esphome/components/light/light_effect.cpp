#include "light_effect.h"
#include "light_state.h"

namespace esphome::light {

uint32_t LightEffect::get_index() const {
  if (this->state_ == nullptr) {
    return 0;
  }
  return this->get_index_in_parent_();
}

bool LightEffect::is_active() const {
  if (this->state_ == nullptr) {
    return false;
  }
  return this->get_index() != 0 && this->state_->get_current_effect_index() == this->get_index();
}

uint32_t LightEffect::get_index_in_parent_() const {
  if (this->state_ == nullptr) {
    return 0;
  }

  const auto &effects = this->state_->get_effects();
  for (size_t i = 0; i < effects.size(); i++) {
    if (effects[i] == this) {
      return i + 1;  // Effects are 1-indexed in the API
    }
  }
  return 0;  // Not found
}

}  // namespace esphome::light
