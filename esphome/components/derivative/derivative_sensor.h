#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::derivative {

enum DerivativeSensorTime {
  DERIVATIVE_SENSOR_TIME_MILLISECOND = 0,
  DERIVATIVE_SENSOR_TIME_SECOND,
  DERIVATIVE_SENSOR_TIME_MINUTE,
  DERIVATIVE_SENSOR_TIME_HOUR,
  DERIVATIVE_SENSOR_TIME_DAY,
};

class DerivativeSensor final : public sensor::Sensor, public Component {
  SUB_SENSOR(source)

 public:
  void setup() override;
  void dump_config() override;
  void set_time(DerivativeSensorTime time) { this->time_ = time; }

 protected:
  void process_sensor_value_(float value);
  float get_time_factor_() {
    switch (this->time_) {
      case DERIVATIVE_SENSOR_TIME_MILLISECOND:
        return 1.0f;
      case DERIVATIVE_SENSOR_TIME_SECOND:
        return 1.0f / 1000.0f;
      case DERIVATIVE_SENSOR_TIME_MINUTE:
        return 1.0f / 60000.0f;
      case DERIVATIVE_SENSOR_TIME_HOUR:
        return 1.0f / 3600000.0f;
      case DERIVATIVE_SENSOR_TIME_DAY:
        return 1.0f / 86400000.0f;
      default:
        return 0.0f;
    }
  }

  DerivativeSensorTime time_{DERIVATIVE_SENSOR_TIME_SECOND};

  uint32_t last_update_{0};
  bool has_last_{false};
  float last_value_{0.0f};
};

}  // namespace esphome::derivative
