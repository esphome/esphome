#pragma once

#include "esphome/components/number/number.h"

#include "../duco.h"

namespace esphome::duco {

class DucoManualFlowLevel : public DucoDevice, public number::Number {
 public:
  void update() override;
  void receive_response(const DucoMessage &message) override;

  void set_address(uint8_t address) { this->address_ = address; }
  void set_item(uint8_t item) { this->item_ = item; }

 protected:
  void control(float value) override;

  uint8_t address_{1};
  uint8_t item_{0};
};

}  // namespace esphome::duco
