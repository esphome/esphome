#pragma once

#include "esphome/components/button/button.h"
#include "../ld2412.h"

namespace esphome::ld2412 {

class FactoryResetButton final : public button::Button, public Parented<LD2412Component> {
 public:
  // User provided, not "= default": `new(p) FactoryResetButton()` would zero-fill .bss that is already zero.
  FactoryResetButton() {}

 protected:
  void press_action() override;
};

}  // namespace esphome::ld2412
