#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "text.h"

namespace esphome {
namespace text {

class TextStateTrigger : public Trigger<std::string> {
 public:
  explicit TextStateTrigger(Text *parent) {
    parent->add_on_state_callback([this](const std::string &value) { this->trigger(value); });
  }
};

template<typename... Ts> class TextSetAction : public Action<Ts...> {
 public:
  explicit TextSetAction(Text *text) : text_(text) {}
  TEMPLATABLE_VALUE(std::string, value)

  void play(Ts... x) override {
    auto call = this->text_->make_call();
    call.set_value(this->value_.value(x...));
    call.perform();
  }

 protected:
  Text *text_;
};

}  // namespace text
}  // namespace esphome
