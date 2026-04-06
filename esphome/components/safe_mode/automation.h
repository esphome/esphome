#pragma once
#include "esphome/core/automation.h"
#include "safe_mode.h"

namespace esphome::safe_mode {

template<typename... Ts> class MarkSuccessfulAction : public Action<Ts...>, public Parented<SafeModeComponent> {
 public:
  void play(const Ts &...x) override { this->parent_->mark_successful(); }
};

}  // namespace esphome::safe_mode
