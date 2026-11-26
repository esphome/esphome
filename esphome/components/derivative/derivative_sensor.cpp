#include "derivative_sensor.h"

#include <cmath>

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::derivative {

ESPHOME_LOG_TAG(TAG, "derivative");

void DerivativeSensor::setup() {
  this->last_update_ = App.get_loop_component_start_time();
  this->source_sensor_->add_on_state_callback([this](float state) { this->process_sensor_value_(state); });
}

void DerivativeSensor::dump_config() { LOG_SENSOR("", "Derivative Sensor", this); }

void DerivativeSensor::process_sensor_value_(float value) {
  // A NaN reading means the source has no valid state (a failed measurement, or a gap
  // between samples). Drop the baseline so the next valid reading re-establishes it,
  // instead of deriving a bogus rate across the gap.
  if (std::isnan(value)) {
    this->has_last_ = false;
    return;
  }

  // App.get_loop_component_start_time() is the time the current loop iteration started.
  // Sensor publishes are synchronous, so a value published during this iteration shares
  // the same timestamp as its source, giving an accurate dt down to one loop iteration.
  const uint32_t now = App.get_loop_component_start_time();

  if (!this->has_last_) {
    // First valid reading (ever, or after a gap): only establish the baseline.
    this->last_value_ = value;
    this->last_update_ = now;
    this->has_last_ = true;
    return;
  }

  const uint32_t dt_ms = now - this->last_update_;
  const float old_value = this->last_value_;
  // Update the baseline before publishing: publish_state() runs the entity's callbacks
  // synchronously and they may feed back into this sensor.
  this->last_value_ = value;
  this->last_update_ = now;

  if (dt_ms == 0) {
    // Two readings in the same loop iteration: no time elapsed, nothing to derive.
    return;
  }

  const float dt = dt_ms * this->get_time_factor_();
  this->publish_state((value - old_value) / dt);
}

}  // namespace esphome::derivative
