#pragma once

#include "esphome/components/button/button.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"

namespace esphome {
namespace button {

template<typename... Ts> class PressAction : public Action<Ts...> {
 public:
  explicit PressAction(Button *button) : button_(button) {}

  void play(Ts... x) override { this->button_->press(); }

 protected:
  Button *button_;
};

class ButtonPressTrigger : public Trigger<> {
 public:
  ButtonPressTrigger(Button *button) {
    button->add_on_press_callback([this]() { this->trigger(); });
  }
};

}  // namespace button
}  // namespace esphome
