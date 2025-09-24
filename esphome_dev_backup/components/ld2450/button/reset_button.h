#pragma once

#include "esphome/components/button/button.h"
#include "../ld2450.h"

namespace esphome {
namespace ld2450 {

class ResetButton : public button::Button, public Parented<LD2450Component> {
 public:
  ResetButton() = default;

 protected:
  void press_action() override;
};

}  // namespace ld2450
}  // namespace esphome
