#include "control_mode_text_sensor.h"
#include "esphome/core/log.h"
#include "../equitherm.h"

namespace esphome {
namespace equitherm {

static const char *const TAG = "control_mode_text_sensor";

// Static string literals for control mode - pointer dedup avoids publish_state on unchanged updates
static const char *const MODE_OFF = "Off";
static const char *const MODE_EQUITHERM_PID = "Equitherm + PID";
static const char *const MODE_EQUITHERM_ONLY = "Equitherm Only";
static const char *const MODE_OUTDOOR_FALLBACK = "Outdoor Fallback";
static const char *const MODE_DEGRADED = "Degraded";

void ControlModeTextSensor::setup() {
  this->parent_->add_on_state_callback([this]() { this->update_from_parent_(); });
  this->update_from_parent_();
}

void ControlModeTextSensor::update_from_parent_() {
  const char *mode;

  // Check climate mode FIRST - if off, show Off regardless of sensor states
  if (this->parent_->mode == climate::CLIMATE_MODE_OFF) {
    mode = MODE_OFF;
  } else {
    bool outdoor_fallback = this->parent_->is_outdoor_sensor_fault();
    bool indoor_fallback = this->parent_->is_indoor_sensor_fault();

    if (!outdoor_fallback && !indoor_fallback) {
      // Both sensors valid - check if PID is actually active
      mode = this->parent_->is_pid_active() ? MODE_EQUITHERM_PID : MODE_EQUITHERM_ONLY;
    } else if (outdoor_fallback && !indoor_fallback) {
      mode = MODE_OUTDOOR_FALLBACK;  // Outdoor sensor failed, PID still active
    } else if (!outdoor_fallback && indoor_fallback) {
      mode = MODE_EQUITHERM_ONLY;  // Indoor sensor failed, PID disabled
    } else {
      mode = MODE_DEGRADED;  // Both sensors failed - minimal control
    }
  }

  // Pointer compare - all modes are static literals, so pointer equality = string equality
  if (mode == this->last_mode_)
    return;
  this->last_mode_ = mode;
  this->publish_state(mode);
}

void ControlModeTextSensor::dump_config() { LOG_TEXT_SENSOR("", "ControlModeTextSensor", this); }

}  // namespace equitherm
}  // namespace esphome
