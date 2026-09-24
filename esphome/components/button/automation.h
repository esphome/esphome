#pragma once

#include "esphome/components/button/button.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"

namespace esphome::button {

template<typename... Ts> class PressAction final : public Action<Ts...> {
 public:
  explicit PressAction(Button *button) : button_(button) {}

  void play(const Ts &...x) override { this->button_->press(); }

 protected:
  Button *button_;
};

}  // namespace esphome::button
