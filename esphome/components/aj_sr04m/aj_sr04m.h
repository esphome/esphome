#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace aj_sr04m {

class Ajsr04mComponent : public sensor::Sensor, public PollingComponent, public uart::UARTDevice {
 public:
  void setup() override;
  void dump_config() override;

  void update() override;

  float get_setup_priority() const override;
};

}  // namespace aj_sr04m
}  // namespace esphome
