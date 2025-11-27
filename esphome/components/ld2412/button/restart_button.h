#pragma once

#include "esphome/components/button/button.h"
#include "../ld2412.h"

namespace esphome::ld2412 {

class RestartButton : public button::Button, public Parented<LD2412Component> {
 public:
  RestartButton() = default;

 protected:
  void press_action() override;
};

}  // namespace esphome::ld2412
