#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace equitherm {

class EquithermClimate;

enum EquithermSensorType {
  EQUITHERM_SENSOR_TYPE_CURVE_OUTPUT_RAW,       // Raw heating curve output (before PID and rate limiting)
  EQUITHERM_SENSOR_TYPE_BASE_CURVE_OUTPUT,      // Curve + PID, before rate limiting
  EQUITHERM_SENSOR_TYPE_FINAL_FLOW_SETPOINT,    // Final flow temperature (after all corrections)
  EQUITHERM_SENSOR_TYPE_LAST_WRITTEN_SETPOINT,  // Last value actually written to boiler
  EQUITHERM_SENSOR_TYPE_PID_CORRECTION,         // PID correction value
  EQUITHERM_SENSOR_TYPE_PROPORTIONAL_TERM,      // PID proportional term (Kp * error)
  EQUITHERM_SENSOR_TYPE_INTEGRAL_TERM,          // PID integral term
  EQUITHERM_SENSOR_TYPE_DERIVATIVE_TERM,        // PID derivative term
  EQUITHERM_SENSOR_TYPE_FALLBACK_DURATION,      // Duration in fallback mode (seconds)
};

class EquithermSensor : public sensor::Sensor, public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_parent(EquithermClimate *parent) { parent_ = parent; }
  void set_type(EquithermSensorType type) { type_ = type; }

 protected:
  void update_from_parent_();

  EquithermClimate *parent_{nullptr};
  EquithermSensorType type_{};
};

}  // namespace equitherm
}  // namespace esphome
