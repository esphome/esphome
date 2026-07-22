#pragma once
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome::d01 {

class D01Component final : public PollingComponent, public uart::UARTDevice {
 public:
  void set_pm25_sensor(sensor::Sensor *s) { this->pm25_sensor_ = s; }
  void dump_config() override;
  void update() override;

 protected:
  sensor::Sensor *pm25_sensor_{nullptr};
};

}  // namespace d01
  // namespace esphome
