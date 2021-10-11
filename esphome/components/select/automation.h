#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "select.h"

namespace esphome {
namespace select {

class SelectStateTrigger : public Trigger<std::string> {
 public:
  explicit SelectStateTrigger(Select *parent) {
    parent->add_on_state_callback([this](const std::string &value) { this->trigger(value); });
  }
};

template<typename... Ts> class SelectSetAction : public Action<Ts...> {
 public:
  SelectSetAction(Select *select) : select_(select) {}
  TEMPLATABLE_VALUE(std::string, option)

  void play(Ts... x) override {
    auto call = this->select_->make_call();
    call.set_option(this->option_.value(x...));
    call.perform();
  }

 protected:
  Select *select_;
};

}  // namespace select
}  // namespace esphome
