#pragma once

#include "esphome/components/button/button.h"

namespace esphome {
namespace ld2410 {

class VirtualButton : public button::Button {
 public:
  VirtualButton();

 protected:
  void press_action() override;
};

}  // namespace ld2410
}  // namespace esphome
