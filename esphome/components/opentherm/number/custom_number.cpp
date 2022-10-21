#include "custom_number.h"

namespace esphome {
namespace opentherm {

void CustomNumber::setup() {
  float value{NAN};
  if (!this->restore_value_) {
    value = this->initial_value_;
  } else {
    this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
    this->pref_.load(&value);
  }
  if (std::isnan(value)) {
    value = this->traits.get_min_value();
  }
  this->publish_state(value);
}

void CustomNumber::control(float value) {
  this->publish_state(value);

  if (this->restore_value_) {
    this->pref_.save(&value);
  }
};

}  // namespace opentherm
}  // namespace esphome
