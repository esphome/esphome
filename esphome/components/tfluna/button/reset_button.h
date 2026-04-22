#pragma once

#include "esphome/components/button/button.h"
#include "../tfluna.h"

namespace esphome {
namespace tfluna {

class ResetButton : public button::Button, public Parented<TFLuna> {
 public:

 protected:
  void press_action() override;
};

}  // namespace tfluna
}  // namespace esphome
