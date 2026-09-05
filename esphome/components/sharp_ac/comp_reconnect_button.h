#pragma once

#include "comp_climate.h"
#include "esphome/components/button/button.h"

namespace esphome::sharp_ac {

class ReconnectButton : public button::Button, public Parented<SharpAc> {
 protected:
  void press_action() override;
};

}  // namespace esphome::sharp_ac
