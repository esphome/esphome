#include "opentherm42_number.h"

namespace esphome::opentherm42 {

static const char *const TAG = "opentherm42.number";

void OpenTherm42Number::control(float value) {
  ESP_LOGD(TAG, "'%s' commanded to %.2f", this->get_name().c_str(), value);
  this->write_value_ = value;
  this->publish_state(value);
  this->pref_.save(&value);
}

void OpenTherm42Number::setup() {
  float value = this->initial_value_;
  this->pref_ = this->make_entity_preference<float>();
  this->pref_.load(&value);  // keeps initial_value_ on first boot / a corrupt preference
  this->write_value_ = value;
  this->publish_state(value);
}

void OpenTherm42Number::dump_config() { LOG_NUMBER("", "OpenTherm 4.2 Number", this); }

}  // namespace esphome::opentherm42
