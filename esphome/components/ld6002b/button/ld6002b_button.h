#pragma once

#include "esphome/components/button/button.h"
#include "../ld6002b.h"

namespace esphome::ld6002b {

class LD6002BButton : public button::Button, public Parented<LD6002BComponent> {
 public:
  explicit LD6002BButton(ButtonType type) : type_(type) {}

 protected:
  void press_action() override;

  ButtonType type_;
};

}  // namespace esphome::ld6002b
