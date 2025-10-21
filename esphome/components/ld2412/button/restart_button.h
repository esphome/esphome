#pragma once

#include "esphome/components/button/button.h"
#include "../ld2412.h"

namespace esphome {
namespace ld2412 {

class RestartButton : public button::Button, public Parented<LD2412Component> {
 public:
  RestartButton() = default;

 protected:
  void press_action() override;
};

}  // namespace ld2412
}  // namespace esphome
