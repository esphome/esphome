#pragma once

#include "esphome/components/number/number.h"

#include "../duco.h"

namespace esphome::duco {

class DucoComfortTemperature : public DucoDevice, public number::Number {
 public:
  void update() override;
  void receive_response(const DucoMessage &message) override;

  void set_address(uint8_t address) { this->address_ = address; }

 protected:
  void control(float value) override;

  uint8_t address_{12};
};

}  // namespace esphome::duco
