#include "control_mode_text_sensor.h"
#include "esphome/core/log.h"
#include "../equitherm.h"

namespace esphome {
namespace equitherm {

static const char *const TAG = "control_mode_text_sensor";

// Static string literals for control mode - zero heap allocation on state changes
static const char *const MODE_NORMAL = "Normal";
static const char *const MODE_OUTDOOR_FALLBACK = "Outdoor Fallback";
static const char *const MODE_EQUITHERM_ONLY = "Equitherm Only";
static const char *const MODE_DEGRADED = "Degraded";

void ControlModeTextSensor::setup() {
  this->parent_->add_on_state_callback([this]() { this->update_from_parent_(); });
  this->update_from_parent_();
}

void ControlModeTextSensor::update_from_parent_() {
  const char *mode;

  bool outdoor_fallback = this->parent_->is_outdoor_fallback_active();
  bool indoor_fallback = this->parent_->is_indoor_fallback_active();

  if (!outdoor_fallback && !indoor_fallback) {
    mode = MODE_NORMAL;  // Both sensors valid - full equitherm + PID control
  } else if (outdoor_fallback && !indoor_fallback) {
    mode = MODE_OUTDOOR_FALLBACK;  // Outdoor sensor failed, PID still active
  } else if (!outdoor_fallback && indoor_fallback) {
    mode = MODE_EQUITHERM_ONLY;  // Indoor sensor failed, PID disabled
  } else {
    mode = MODE_DEGRADED;  // Both sensors failed - minimal control
  }

  this->publish_state(mode);
}

void ControlModeTextSensor::dump_config() { LOG_TEXT_SENSOR("", "ControlModeTextSensor", this); }

}  // namespace equitherm
}  // namespace esphome
