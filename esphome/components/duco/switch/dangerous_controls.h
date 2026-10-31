#pragma once

#include "esphome/components/switch/switch.h"

#include "../duco.h"

namespace esphome::duco {

class DucoDangerousControls : public switch_::Switch, public Component, public Parented<Duco> {
 public:
  void setup() override;
  void write_state(bool state) override;
};

}  // namespace esphome::duco
