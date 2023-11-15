#pragma once

#include "esphome/components/button/button.h"
#include "../opentherm.h"

namespace esphome {
namespace opentherm {

class CHWaterFillingButton : public button::Button, public Parented<OpenThermComponent> {
 public:
  CHWaterFillingButton() = default;

 protected:
  void press_action() override;
};

}  // namespace opentherm
}  // namespace esphome
