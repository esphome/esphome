#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/component.h"
#include "../hoermann.h"

namespace esphome::hoermann {

class HoermannConnectedBinarySensor : public binary_sensor::BinarySensor, public Component {
 public:
  void setup() override;
  void set_parent(Hoermann *parent) { this->parent_ = parent; }

 protected:
  void update_from_state_();
  Hoermann *parent_{nullptr};
};

}  // namespace esphome::hoermann
