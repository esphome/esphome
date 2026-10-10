#pragma once

#include "esphome/components/button/button.h"
#include "../ld2460.h"

namespace esphome::ld2460 {

class FactoryResetButton : public button::Button, public Parented<LD2460Component> {
 public:
  FactoryResetButton() = default;

 protected:
  void press_action() override;
};

}  // namespace esphome::ld2460
