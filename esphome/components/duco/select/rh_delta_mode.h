#pragma once

#include "esphome/components/select/select.h"

#include "../duco.h"

namespace esphome::duco {

class DucoRhDeltaMode : public DucoDevice, public select::Select {
 public:
  void update() override;
  void receive_response(const DucoMessage &message) override;

  void set_address(uint8_t address) { this->address_ = address; }

 protected:
  void control(size_t index) override;

  uint8_t address_{1};
};

}  // namespace esphome::duco
