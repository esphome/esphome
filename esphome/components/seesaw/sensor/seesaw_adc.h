#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "../seesaw.h"

namespace esphome::seesaw {

class SeesawADC : public sensor::Sensor, public PollingComponent {
 public:
  void setup() override;
  void update() override;

  void set_parent(Seesaw *parent) { this->parent_ = parent; }
  void set_pin(uint8_t pin) { this->pin_ = pin; }

 protected:
  Seesaw *parent_;
  uint8_t pin_;
};

}  // namespace esphome::seesaw
