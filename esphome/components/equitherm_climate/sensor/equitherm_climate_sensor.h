#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace equitherm_climate {

class EquithermClimate;

enum EquithermClimateSensorType {
  EQUITHERM_SENSOR_TYPE_FLOW_CURVE,      // Base equitherm curve output (before corrections)
  EQUITHERM_SENSOR_TYPE_FLOW_FINAL,      // Final flow temperature (after all corrections)
  EQUITHERM_SENSOR_TYPE_PID_CORRECTION,  // PID correction value
};

class EquithermClimateSensor : public sensor::Sensor, public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_parent(EquithermClimate *parent) { parent_ = parent; }
  void set_type(EquithermClimateSensorType type) { type_ = type; }

 protected:
  void update_from_parent_();

  EquithermClimate *parent_;
  EquithermClimateSensorType type_;
};

}  // namespace equitherm_climate
}  // namespace esphome
