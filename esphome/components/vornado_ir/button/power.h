#pragma once

#include "esphome/components/button/button.h"
#include "../vornado_ir.h"

namespace esphome::vornado_ir {

class PowerButton : public button::Button, public Parented<VornadoIR> {
 public:
  PowerButton() = default;

 protected:
  void press_action() override;
};

}  // namespace esphome::vornado_ir
