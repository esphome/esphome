#pragma once
#include "esphome/core/defines.h"
#include "esphome/core/automation.h"
#include "safe_mode.h"

namespace esphome::safe_mode {

#ifdef USE_SAFE_MODE_CALLBACK
class SafeModeTrigger final : public Trigger<> {
 public:
  explicit SafeModeTrigger(SafeModeComponent *parent) {
    parent->add_on_safe_mode_callback([this]() { trigger(); });
  }
};
#endif  // USE_SAFE_MODE_CALLBACK

template<typename... Ts> class MarkBootOkAction : public Action<Ts...> {
 public:
  explicit MarkBootOkAction(SafeModeComponent *parent) : parent_(parent) {}
  void play(const Ts &...x) override { this->parent_->mark_boot_ok(); }

 protected:
  SafeModeComponent *parent_;
};

}  // namespace esphome::safe_mode
