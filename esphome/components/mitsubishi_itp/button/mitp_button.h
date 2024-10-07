#pragma once

#include "esphome/components/button/button.h"
#include "../mitsubishi_itp.h"

namespace esphome {
namespace mitsubishi_itp {

class MITPButton : public button::Button, public Component, public Parented<MitsubishiUART> {
 public:
  MITPButton() = default;
  using Parented<MitsubishiUART>::Parented;

 protected:
  virtual void press_action() override = 0;
};

class FilterResetButton : public MITPButton {
 protected:
  void press_action() override { this->parent_->reset_filter_status(); }
};

}  // namespace mitsubishi_itp
}  // namespace esphome
