#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

#include "../rojaflex.h"

namespace esphome::rojaflex {

enum class RojaflexSensorType : uint8_t {
  MOTOR_PCT,
};

class RojaflexSensor : public sensor::Sensor, public PollingComponent, public RojaflexDevice {
 public:
  void set_sensor_type(RojaflexSensorType type) { this->type_ = type; }
  void set_channel(uint8_t channel) { this->channel_ = channel; }
  void update() override;

 protected:
  RojaflexSensorType type_{RojaflexSensorType::MOTOR_PCT};
  uint8_t channel_{0};
};

}  // namespace esphome::rojaflex
