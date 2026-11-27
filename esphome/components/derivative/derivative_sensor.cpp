#include "derivative_sensor.h"

#include <cmath>

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::derivative {

ESPHOME_LOG_TAG(TAG, "derivative");

void DerivativeSensorBase::dump_config() { LOG_SENSOR("", "Derivative Sensor", this); }

void DerivativeSensorBase::process_sensor_value_(float value, float ms_per_unit) {
  // No valid reading: drop the baseline so a gap never yields a spike.
  if (std::isnan(value)) {
    this->last_value_ = NAN;
    return;
  }

  const uint32_t now = App.get_loop_component_start_time();

  if (std::isnan(this->last_value_)) {
    this->last_value_ = value;
    this->last_update_ = now;
    return;
  }

  const uint32_t dt_ms = now - this->last_update_;
  // Readings published within one component call share a timestamp: keep the older baseline.
  if (dt_ms == 0)
    return;

  const float old_value = this->last_value_;
  // Update before publishing: callbacks may feed back into this sensor.
  this->last_value_ = value;
  this->last_update_ = now;

  this->publish_state((value - old_value) / dt_ms * ms_per_unit);
}

}  // namespace esphome::derivative
