#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace ajsr04m {

class AJSR04MComponent : public Component, public uart::UARTDevice {
 public:
  float get_setup_priority() const override;

  void loop() override;
  void dump_config() override;

  void set_distance_sensor(sensor::Sensor *distance_sensor) { this->distance_sensor_ = distance_sensor; }

 protected:
  sensor::Sensor *distance_sensor_{nullptr};
};

}  // namespace ajsr04m
}  // namespace esphome
