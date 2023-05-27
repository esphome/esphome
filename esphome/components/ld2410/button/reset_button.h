#pragma once

#include "esphome/components/button/button.h"
#include "../ld2410.h"

namespace esphome {
namespace ld2410 {

class ResetButton : public button::Button, public Parented<LD2410Component> {
 public:
  ResetButton() = default;

 protected:
  void press_action() override;
};

}  // namespace ld2410
}  // namespace esphome
