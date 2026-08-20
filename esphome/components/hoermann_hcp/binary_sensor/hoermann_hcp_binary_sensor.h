#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/component.h"
#include "../hoermann_hcp.h"

namespace esphome::hoermann_hcp {

class HoermannHcpConnectedBinarySensor : public binary_sensor::BinarySensor, public Component {
 public:
  explicit HoermannHcpConnectedBinarySensor(HoermannHcp *parent) : parent_(parent) {}

  void setup() override;
  void dump_config() override;

 protected:
  HoermannHcp *const parent_;
};

}  // namespace esphome::hoermann_hcp
