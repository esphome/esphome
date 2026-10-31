#pragma once

#include "esphome/components/switch/switch.h"

#include "../duco.h"

namespace esphome::duco {

class DucoInstallerMode : public switch_::Switch, public Parented<Duco> {
 public:
  void write_state(bool state) override;
};

}  // namespace esphome::duco
