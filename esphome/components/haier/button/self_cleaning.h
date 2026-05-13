#pragma once

#include "esphome/components/button/button.h"
#include "../hon_climate.h"

namespace esphome::haier {

class SelfCleaningButton : public button::Button, public Parented<HonClimate> {
 public:
  SelfCleaningButton() = default;

 protected:
  void press_action() override;
};

}  // namespace esphome::haier
