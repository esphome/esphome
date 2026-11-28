#pragma once

#include "esphome/components/number/number.h"

#include "../duco.h"

namespace esphome::duco {

class DucoSetpoint final : public DucoDevice, public number::Number {
 public:
  DucoSetpoint(uint8_t address, uint8_t item) : address_(address), item_(item) {}
  void update() override;
  void receive_response(const DucoMessage &message) override;

 protected:
  void control(float value) override;
  uint8_t address_;
  uint8_t item_;
};

}  // namespace esphome::duco
