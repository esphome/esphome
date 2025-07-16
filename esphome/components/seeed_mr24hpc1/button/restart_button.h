#pragma once

#include "esphome/components/button/button.h"
#include "../seeed_mr24hpc1.h"

namespace esphome {
namespace seeed_mr24hpc1 {

class RestartButton : public button::Button, public Parented<MR24HPC1Component> {
 public:
  RestartButton() = default;

 protected:
  void press_action() override;
};

}  // namespace seeed_mr24hpc1
}  // namespace esphome
