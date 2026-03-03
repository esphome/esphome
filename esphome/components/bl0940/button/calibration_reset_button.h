#pragma once

#include "esphome/core/component.h"
#include "esphome/components/button/button.h"

namespace esphome {
namespace bl0940 {

class BL0940;  // Forward declaration of BL0940 class

class CalibrationResetButton : public button::Button, public Component, public Parented<BL0940> {
 public:
  void dump_config() override;

  void press_action() override;
};

}  // namespace bl0940
}  // namespace esphome
