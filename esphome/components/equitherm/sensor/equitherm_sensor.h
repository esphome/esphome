#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace equitherm {

class EquithermClimate;

enum EquithermSensorType {
  EQUITHERM_SENSOR_TYPE_CURVE_OUTPUT_RAW,       // Raw heating curve output (before rate limiting)
  EQUITHERM_SENSOR_TYPE_BASE_CURVE_OUTPUT,      // After rate limiting, before PID correction
  EQUITHERM_SENSOR_TYPE_FINAL_FLOW_SETPOINT,    // Final flow temperature (after all corrections)
  EQUITHERM_SENSOR_TYPE_LAST_WRITTEN_SETPOINT,  // Last value actually written to boiler
  EQUITHERM_SENSOR_TYPE_PID_CORRECTION,         // PID correction value
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
