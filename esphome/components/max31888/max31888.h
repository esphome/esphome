#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/one_wire/one_wire.h"

namespace esphome::max31888 {

class MAX31888Sensor final : public PollingComponent, public sensor::Sensor, public one_wire::OneWireDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;

 protected:
  /// Reads the latest conversion from the FIFO; false when the bus or the checksum fails
  bool read_temperature_(int16_t &raw);
};

}  // namespace esphome::max31888
