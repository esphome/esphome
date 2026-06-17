#pragma once

#include "esphome/components/button/button.h"
#include "../ld2410.h"

namespace esphome::ld2410 {

class FactoryResetButton : public button::Button, public Parented<LD2410Component> {
 public:
  FactoryResetButton() = default;

 protected:
  void press_action() override;
};

}  // namespace esphome::ld2410
