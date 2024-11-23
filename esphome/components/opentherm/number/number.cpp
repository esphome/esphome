#include "number.h"

namespace esphome {
namespace opentherm {

static const char *const TAG = "opentherm.number";

void OpenthermNumber::control(float value) {
  this->publish_state(value);

  if (this->restore_value_)
    this->pref_.save(&value);
}

void OpenthermNumber::setup() {
  float value;
  if (!this->restore_value_) {
    value = this->initial_value_;
  } else {
    this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
    if (!this->pref_.load(&value)) {
      if (!std::isnan(this->initial_value_)) {
        value = this->initial_value_;
      } else {
        value = this->traits.get_min_value();
      }
    }
  }
  this->publish_state(value);
}

void OpenthermNumber::dump_config() {
  LOG_NUMBER("", "OpenTherm Number", this);
  ESP_LOGCONFIG(TAG, "  Restore value: %d", this->restore_value_);
  ESP_LOGCONFIG(TAG, "  Initial value: %.2f", this->initial_value_);
  ESP_LOGCONFIG(TAG, "  Current value: %.2f", this->state);
}

}  // namespace opentherm
}  // namespace esphome
