#pragma once

#include "esphome/components/button/button.h"
#include "../seeed_mr60fda2.h"

namespace esphome {
namespace seeed_mr60fda2 {

class ResetRadarButton : public button::Button, public Parented<MR60FDA2Component> {
 public:
  ResetRadarButton() = default;

 protected:
  void press_action() override;
};

}  // namespace seeed_mr60fda2
}  // namespace esphome
