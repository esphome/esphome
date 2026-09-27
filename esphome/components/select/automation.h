#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "select.h"

namespace esphome::select {

class SelectStateTrigger final : public Trigger<StringRef, size_t> {
 public:
  explicit SelectStateTrigger(Select *parent) : parent_(parent) {
    parent->add_on_state_callback(
        [this](size_t index) { this->trigger(StringRef(this->parent_->option_at(index)), index); });
  }

 protected:
  Select *parent_;
};

template<size_t N, typename... Ts> class SelectIsCondition final : public Condition<Ts...> {
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

template<typename... Ts> class SelectIsCondition<0, Ts...> final : public Condition<Ts...> {
 public:
  SelectIsCondition(Select *parent, std::function<bool(const StringRef &, const Ts &...)> &&f)
      : parent_(parent), f_(f) {}

  bool check(const Ts &...x) override { return this->f_(this->parent_->current_option(), x...); }

 protected:
  Select *parent_;
  std::function<bool(const StringRef &, const Ts &...)> f_;
};
}  // namespace esphome::select
