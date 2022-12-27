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
  TEMPLATABLE_VALUE(std::string, option)

  void play(Ts... x) override {
    auto call = this->text_->make_call();
    call.set_option(this->option_.value(x...));
    call.perform();
  }

 protected:
  Text *text_;
};

template<typename... Ts> class TextSetIndexAction : public Action<Ts...> {
 public:
  explicit TextSetIndexAction(Text *text) : text_(text) {}
  TEMPLATABLE_VALUE(size_t, index)

  void play(Ts... x) override {
    auto call = this->text_->make_call();
    call.set_index(this->index_.value(x...));
    call.perform();
  }

 protected:
  Text *text_;
};

template<typename... Ts> class TextOperationAction : public Action<Ts...> {
 public:
  explicit TextOperationAction(Text *text) : text_(text) {}
  TEMPLATABLE_VALUE(bool, cycle)
  TEMPLATABLE_VALUE(TextOperation, operation)

  void play(Ts... x) override {
    auto call = this->text_->make_call();
    call.with_operation(this->operation_.value(x...));
    if (this->cycle_.has_value()) {
      call.with_cycle(this->cycle_.value(x...));
    }
    call.perform();
  }

 protected:
  Text *text_;
};

}  // namespace text
}  // namespace esphome
