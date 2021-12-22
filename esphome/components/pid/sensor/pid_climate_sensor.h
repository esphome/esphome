#pragma once

#include "esphome/core/component.h"
#include "esphome/components/pid/pid_climate.h"

namespace esphome {
namespace pid {

enum PIDClimateSensorType {
  PID_SENSOR_TYPE_RESULT,
  PID_SENSOR_TYPE_ERROR,
  PID_SENSOR_TYPE_PROPORTIONAL,
  PID_SENSOR_TYPE_INTEGRAL,
  PID_SENSOR_TYPE_DERIVATIVE,
  PID_SENSOR_TYPE_HEAT,
  PID_SENSOR_TYPE_COOL,
  PID_SENSOR_TYPE_KP,
  PID_SENSOR_TYPE_KI,
  PID_SENSOR_TYPE_KD,
};

class PIDClimateSensor : public sensor::Sensor, public Component {
 public:
  void setup() override;
  void set_parent(PIDClimate *parent) { parent_ = parent; }
  void set_type(PIDClimateSensorType type) { type_ = type; }

  void dump_config() override;

 protected:
  void update_from_parent_();
  PIDClimate *parent_;
  PIDClimateSensorType type_;
};

}  // namespace pid
}  // namespace esphome
