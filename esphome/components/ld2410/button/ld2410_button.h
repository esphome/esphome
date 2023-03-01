#pragma once

#include "esphome/components/button/button.h"

namespace esphome {
namespace ld2410 {

class LD2410Button : public button::Button {
 public:
  LD2410Button();

 protected:
  void press_action() override;
};

}  // namespace ld2410
}  // namespace esphome
