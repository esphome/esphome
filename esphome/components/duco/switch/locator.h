#pragma once

#include "esphome/components/switch/switch.h"

#include "../duco.h"

namespace esphome::duco {

class DucoLocator : public switch_::Switch, public Parented<Duco> {
 public:
  void set_address(uint8_t address) { this->address_ = address; }
  void write_state(bool state) override;

 protected:
  uint8_t address_{1};
};

}  // namespace esphome::duco
