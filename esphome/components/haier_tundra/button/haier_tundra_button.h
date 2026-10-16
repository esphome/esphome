#pragma once

#include "esphome/core/component.h"
#include "esphome/components/button/button.h"
#include "esphome/components/haier_tundra/haier_tundra.h"

namespace esphome::haier_tundra {

enum class ButtonKind : uint8_t {
  BUTTON_KIND_LIGHT,
  BUTTON_KIND_SELF_CLEAN,
};

class HaierTundraButton final : public button::Button, public Component, public Parented<HaierTundra> {
 public:
  HaierTundraButton(HaierTundra *parent, ButtonKind kind) : Parented(parent), kind_(kind) {}
  void dump_config() override;

 protected:
  void press_action() override;

  ButtonKind kind_;
};

}  // namespace esphome::haier_tundra
