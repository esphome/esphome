#pragma once

#include "esphome/components/button/button.h"
#include "../mr24hpc1.h"

namespace esphome {
namespace mr24hpc1 {

class CustomSetEndButton : public button::Button, public Parented<mr24hpc1Component> {
  public:
    CustomSetEndButton() = default;

  protected:
    void press_action() override;
};

}  // namespace mr24hpc1
}  // namespace esphome
