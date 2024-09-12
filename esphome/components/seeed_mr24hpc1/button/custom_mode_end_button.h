#pragma once

#include "esphome/components/button/button.h"
#include "../seeed_mr24hpc1.h"

namespace esphome {
namespace seeed_mr24hpc1 {

class CustomSetEndButton : public button::Button, public Parented<MR24HPC1Component> {
 public:
  CustomSetEndButton() = default;

 protected:
  void press_action() override;
};

}  // namespace seeed_mr24hpc1
}  // namespace esphome
