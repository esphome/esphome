#include "esphome/core/log.h"
#include "custom_number.h"
#include "../consts.h"

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

void CustomNumber::dump_custom_config(const char *prefix, const char *type) {
  LOG_NUMBER(prefix, type, this);
  if (!std::isnan(this->initial_value_)) {
    ESP_LOGCONFIG(TAG, "%s  Initial value: '%f'", prefix, this->initial_value_);
  }
  if (!std::isnan(this->restore_value_)) {
    ESP_LOGCONFIG(TAG, "%s  Restore value: '%s'", prefix, YESNO(this->restore_value_));
  }
}

}  // namespace opentherm
}  // namespace esphome
