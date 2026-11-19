#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/sensirion_common/i2c_sensirion.h"

namespace esphome::sdp3x {

class SDP3XComponent final : public PollingComponent,
                             public sensirion_common::SensirionI2CDevice,
                             public sensor::Sensor {
 public:
  /// start_command selects the measurement mode (differential pressure or mass flow).
  explicit SDP3XComponent(uint16_t start_command) : start_command_(start_command) {}

  void update() override;
  void setup() override;
  void dump_config() override;

 protected:
  uint16_t start_command_;
  uint16_t product_id_{0};
};

}  // namespace esphome::sdp3x
