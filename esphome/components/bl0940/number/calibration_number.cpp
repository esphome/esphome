#include "calibration_number.h"
#include "esphome/core/log.h"

namespace esphome {
namespace bl0940 {

static const char *const TAG = "bl0940.number";

void CalibrationNumber::setup() {
  float value = 0.0f;
  if (this->restore_value_) {
    this->pref_ = global_preferences->make_preference<float>(this->get_preference_hash());
    if (!this->pref_.load(&value)) {
      value = 0.0f;
    }
  }
  this->publish_state(value);
}

void CalibrationNumber::control(float value) {
  this->publish_state(value);
  if (this->restore_value_)
    this->pref_.save(&value);
}

void CalibrationNumber::dump_config() { LOG_NUMBER("", "Calibration Number", this); }

}  // namespace bl0940
}  // namespace esphome
