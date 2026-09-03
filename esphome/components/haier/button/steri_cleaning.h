#pragma once

#include "esphome/components/button/button.h"
#include "../hon_climate.h"

namespace esphome::haier {

class SteriCleaningButton final : public button::Button, public Parented<HonClimate> {
 public:
  SteriCleaningButton() = default;

 protected:
  void press_action() override;
};

}  // namespace esphome::haier
