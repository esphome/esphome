#include "equitherm_binary_sensor_base.h"

namespace esphome::equitherm {

void EquithermBinarySensorBase::init_state_(bool value) {
  if (this->restore_value_) {
    this->pref_ = this->make_entity_preference<bool>();
    this->pref_.load(&value);  // overwrites on success, keeps default on failure
  }
  this->publish_state(value);
}

void EquithermBinarySensorBase::save_state_(bool value) {
  if (this->restore_value_) {
    this->pref_.save(&value);
  }
}

}  // namespace esphome::equitherm
