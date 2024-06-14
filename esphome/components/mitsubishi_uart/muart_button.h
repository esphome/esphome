#pragma once

#include "esphome/components/button/button.h"
#include "mitsubishi_uart.h"

namespace esphome {
namespace mitsubishi_uart {

class MUARTButton : public button::Button, public Component, public Parented<MitsubishiUART> {
 public:
  MUARTButton() = default;
  using Parented<MitsubishiUART>::Parented;

 protected:
  virtual void press_action() override;
};

class FilterResetButton : public MUARTButton {
 protected:
  void press_action() { this->parent_->reset_filter_status(); }
};

}  // namespace mitsubishi_uart
}  // namespace esphome
