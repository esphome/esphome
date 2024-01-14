#pragma once

#include "esphome/components/button/button.h"
#include "../opentherm.h"

namespace esphome {
namespace opentherm {

class OpenThermCHWaterFillingButton : public button::Button, public Parented<OpenThermComponent> {
 public:
  OpenThermCHWaterFillingButton() = default;

 protected:
  void press_action() override;
};

}  // namespace opentherm
}  // namespace esphome
