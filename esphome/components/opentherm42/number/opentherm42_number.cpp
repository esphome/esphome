#include "opentherm42_number.h"

namespace esphome::opentherm42 {

static const char *const TAG = "opentherm42.number";

void OpenTherm42Number::control(float value) {
  this->publish_state(value);
  if (this->restore_value_) {
    this->pref_.save(&value);
  }
}

void OpenTherm42Number::setup() {
  float value = this->initial_value_;
  if (this->restore_value_) {
    this->pref_ = this->make_entity_preference<float>();
    this->pref_.load(&value);  // keeps initial_value_ on first boot / a corrupt preference
  }
  this->publish_state(value);
}

void OpenTherm42Number::dump_config() {
  LOG_NUMBER("", "OpenTherm 4.2 Number", this);
  ESP_LOGCONFIG(TAG, "  Restore value: %s", YESNO(this->restore_value_));
}

}  // namespace esphome::opentherm42
