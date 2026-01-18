#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "select.h"

namespace esphome::select {

class SelectStateTrigger : public Trigger<std::string, size_t> {
 public:
  explicit SelectStateTrigger(Select *parent) : parent_(parent) {
    parent->add_on_state_callback(
        [this](size_t index) { this->trigger(std::string(this->parent_->option_at(index)), index); });
  }

 protected:
  Select *parent_;
};

template<typename... Ts> class SelectSetAction : public Action<Ts...> {
 public:
  explicit SelectSetAction(Select *select) : select_(select) {}
  TEMPLATABLE_VALUE(std::string, option)

  void play(const Ts &...x) override {
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

  void play(const Ts &...x) override {
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

  void play(const Ts &...x) override {
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

template<size_t N, typename... Ts> class SelectIsCondition : public Condition<Ts...> {
 public:
  SelectIsCondition(Select *parent, const char *const *option_list) : parent_(parent), option_list_(option_list) {}

  bool check(const Ts &...x) override {
    auto current = this->parent_->current_option();
    for (size_t i = 0; i != N; i++) {
      if (current == this->option_list_[i]) {
        return true;
      }
    }
    return false;
  }

 protected:
  Select *parent_;
  const char *const *option_list_;
};

template<typename... Ts> class SelectIsCondition<0, Ts...> : public Condition<Ts...> {
 public:
  SelectIsCondition(Select *parent, std::function<bool(const StringRef &, const Ts &...)> &&f)
      : parent_(parent), f_(f) {}

  bool check(const Ts &...x) override { return this->f_(this->parent_->current_option(), x...); }

 protected:
  Select *parent_;
  std::function<bool(const StringRef &, const Ts &...)> f_;
};
}  // namespace esphome::select
