#include "number.h"

namespace esphome {
namespace opentherm {

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
  const char *tag = "opentherm.number";
  ESP_LOGCONFIG(tag, "%s%s '%s'", "", LOG_STR_LITERAL("OpenTherm Number"), this->get_name().c_str());
  if (!this->get_icon().empty()) {
    ESP_LOGCONFIG(tag, "%s  Icon: '%s'", "", this->get_icon().c_str());
  }
  if (!this->traits.get_unit_of_measurement().empty()) {
    ESP_LOGCONFIG(tag, "%s  Unit of Measurement: '%s'", "", this->traits.get_unit_of_measurement().c_str());
  }
  if (!this->traits.get_device_class().empty()) {
    ESP_LOGCONFIG(tag, "%s  Device Class: '%s'", "", this->traits.get_device_class().c_str());
  }
  ESP_LOGCONFIG(tag, "  Restore value: %d", this->restore_value_);
  ESP_LOGCONFIG(tag, "  Initial value: %.2f", this->initial_value_);
  ESP_LOGCONFIG(tag, "  Current value: %.2f", this->state);
}

}  // namespace opentherm
}  // namespace esphome
