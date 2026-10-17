#pragma once

#include "esphome/components/button/button.h"
#include "../tfluna.h"

namespace esphome::tfluna {

class RestartButton : public button::Button, public Parented<TFLuna> {
 protected:
  void press_action() override;
};

}  // namespace esphome::tfluna
