#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "select.h"

namespace esphome {
namespace select {

class SelectStateTrigger : public Trigger<std::string, size_t> {
 public:
  explicit SelectStateTrigger(Select *parent) {
    parent->add_on_state_callback([this](const std::string &value, size_t index) { this->trigger(value, index); });
  }
};

template<typename... Ts> class SelectSetAction : public Action<Ts...> {
 public:
  explicit SelectSetAction(Select *select) : select_(select) {}
  TEMPLATABLE_VALUE(std::string, option)

  void play(Ts... x) override {
    auto call = this->select_->make_call();
    call.set_option(this->option_.value(x...));
    call.perform();
  }

 protected:
  Select *select_;
};

template<typename... Ts> class SelectSetIndexAction : public Action<Ts...> {
 public:
  explicit SelectSetIndexAction(Select *select) : select_(select) {}
  TEMPLATABLE_VALUE(size_t, index)

  void play(Ts... x) override {
    auto call = this->select_->make_call();
    call.set_index(this->index_.value(x...));
    call.perform();
  }

 protected:
  Select *select_;
};

template<typename... Ts> class SelectOperationAction : public Action<Ts...> {
 public:
  explicit SelectOperationAction(Select *select) : select_(select) {}
  TEMPLATABLE_VALUE(bool, cycle)
  TEMPLATABLE_VALUE(SelectOperation, operation)

  void play(Ts... x) override {
    auto call = this->select_->make_call();
    call.with_operation(this->operation_.value(x...));
    if (this->cycle_.has_value()) {
      call.with_cycle(this->cycle_.value(x...));
    }
    call.perform();
  }

 protected:
  Select *select_;
};

}  // namespace select
}  // namespace esphome
