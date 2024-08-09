#pragma once

#include "esphome/components/button/button.h"
#include "../mitsubishi_uart.h"

namespace esphome {
namespace mitsubishi_itp {

class MUARTButton : public button::Button, public Component, public Parented<MitsubishiUART> {
 public:
  MUARTButton() = default;
  using Parented<MitsubishiUART>::Parented;

 protected:
  virtual void press_action() override = 0;
};

class FilterResetButton : public MUARTButton {
 protected:
  void press_action() override { this->parent_->reset_filter_status(); }
};

}  // namespace mitsubishi_itp
}  // namespace esphome
