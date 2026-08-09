#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/component.h"
#include "../hoermann_hcp.h"

namespace esphome::hoermann_hcp {

class HoermannHcpConnectedBinarySensor : public binary_sensor::BinarySensor, public Component {
 public:
  void setup() override;
  void set_parent(HoermannHcp *parent) { this->parent_ = parent; }

 protected:
  void update_from_state_();
  HoermannHcp *parent_{nullptr};
};

}  // namespace esphome::hoermann_hcp
