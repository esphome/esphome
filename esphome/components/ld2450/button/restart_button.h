#pragma once

#include "esphome/components/button/button.h"
#include "../ld2450.h"

namespace esphome::ld2450 {

class RestartButton : public button::Button, public Parented<LD2450Component> {
 public:
  RestartButton() = default;

 protected:
  void press_action() override;
};

}  // namespace esphome::ld2450
