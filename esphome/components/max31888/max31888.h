#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/one_wire/one_wire.h"

namespace esphome::max31888 {

class MAX31888Sensor : public PollingComponent, public sensor::Sensor, public one_wire::OneWireDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;

 protected:
  uint8_t fifo_[2] = {0};

  bool read_fifo_();
};

}  // namespace esphome::max31888
