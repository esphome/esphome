#pragma once

#include "esphome/components/button/button.h"
#include "../ld2412.h"

namespace esphome::ld2412 {

class FactoryResetButton : public button::Button, public Parented<LD2412Component> {
 public:
  FactoryResetButton() = default;

 protected:
  void press_action() override;
};

}  // namespace esphome::ld2412
