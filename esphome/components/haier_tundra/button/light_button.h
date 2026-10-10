#pragma once

#include "esphome/core/component.h"
#include "esphome/components/button/button.h"
#include "esphome/components/haier_tundra/haier_tundra.h"

namespace esphome::haier_tundra {

class LightButton final : public button::Button, public Component, public Parented<HaierTundra> {
 public:
  void dump_config() override;

 protected:
  void press_action() override;
};

}  // namespace esphome::haier_tundra
