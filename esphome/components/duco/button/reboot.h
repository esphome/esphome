#pragma once

#include "esphome/components/button/button.h"

#include "../duco.h"

namespace esphome::duco {

class DucoReboot : public button::Button, public Parented<Duco> {
 public:
  void press_action() override;
};

}  // namespace esphome::duco
