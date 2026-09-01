#pragma once
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome::d01 {

class D01SensorComponent final : public sensor::Sensor, public Component, public uart::UARTDevice {
 public:
  void dump_config() override;
  void loop() override;
};

}  // namespace esphome::d01
