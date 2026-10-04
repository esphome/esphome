#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::equitherm {

class EquithermClimate;

enum EquithermSensorType {
  EQUITHERM_SENSOR_TYPE_HEATING_CURVE_OUTPUT,  // Raw heating curve output (before PID and rate limiting)
  EQUITHERM_SENSOR_TYPE_PID_ADJUSTED_OUTPUT,   // Curve + PID, before rate limiting
  EQUITHERM_SENSOR_TYPE_FLOW_SETPOINT,         // Flow temperature (after all corrections)
  EQUITHERM_SENSOR_TYPE_ACTIVE_SETPOINT,       // Last value actually written to boiler (confirmed active)
  EQUITHERM_SENSOR_TYPE_PID_CORRECTION,        // PID correction value
  EQUITHERM_SENSOR_TYPE_PID_PROPORTIONAL,      // PID proportional term (Kp * error)
  EQUITHERM_SENSOR_TYPE_PID_INTEGRAL,          // PID integral term
  EQUITHERM_SENSOR_TYPE_PID_DERIVATIVE,        // PID derivative term
  EQUITHERM_SENSOR_TYPE_MIN_FLOW_TEMP,         // Minimum flow temperature (output parameter)
  EQUITHERM_SENSOR_TYPE_MAX_FLOW_TEMP,         // Maximum flow temperature (output parameter)
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

}  // namespace esphome::equitherm
