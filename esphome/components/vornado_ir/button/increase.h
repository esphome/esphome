#pragma once

#include "esphome/components/button/button.h"
#include "../vornado_ir.h"

namespace esphome::vornado_ir {

class IncreaseButton : public button::Button, public Parented<VornadoIR> {
 public:
  IncreaseButton() = default;

 protected:
  void press_action() override;
};

}  // namespace esphome::vornado_ir
