#include "equitherm_number_base.h"

namespace esphome::equitherm {

void EquithermNumberBase::init_state_(float value) {
  if (this->restore_value_) {
    this->pref_ = this->make_entity_preference<float>();
    this->pref_.load(&value);  // overwrites on success, keeps default on failure
  }
  this->publish_state(value);
}

void EquithermNumberBase::save_state_(float value) {
  if (this->restore_value_) {
    this->pref_.save(&value);
  }
}

}  // namespace esphome::equitherm
