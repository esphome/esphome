#pragma once

#include "esphome/components/button/button.h"

#include "../duco.h"

namespace esphome::duco {

class DucoReboot : public button::Button, public Parented<Duco> {
 public:
  void press_action() override;

  void set_address(uint8_t address) { this->address_ = address; }

 protected:
  uint8_t address_{0};
};

}  // namespace esphome::duco
