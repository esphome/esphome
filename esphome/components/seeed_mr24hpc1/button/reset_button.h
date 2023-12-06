#pragma once

#include "esphome/components/button/button.h"
#include "../seeed_mr24hpc1.h"

namespace esphome {
namespace seeed_mr24hpc1 {

class ResetButton : public button::Button, public Parented<mr24hpc1Component> {
 public:
  ResetButton() = default;

 protected:
  void press_action() override;
};

}  // namespace seeed_mr24hpc1
}  // namespace esphome
