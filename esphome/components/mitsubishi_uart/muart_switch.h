#pragma once

#include "esphome/components/switch/switch.h"
#include "mitsubishi_uart.h"

namespace esphome {
namespace mitsubishi_uart {

class MUARTSwitch : public switch_::Switch, public Component, public Parented<MitsubishiUART> {
 public:
  MUARTSwitch() = default;
  using Parented<MitsubishiUART>::Parented;
  void setup() override {
    this->state = this->get_initial_state_with_restore_mode().value_or(false);
    this->parent_->set_active_mode(state);
  };

 protected:
  virtual void write_state(bool state) override;
};

class ActiveModeSwitch : public MUARTSwitch {
 protected:
  void write_state(bool new_state) {
    this->publish_state(new_state);
    this->parent_->set_active_mode(new_state);
  }
};

}  // namespace mitsubishi_uart
}  // namespace esphome
