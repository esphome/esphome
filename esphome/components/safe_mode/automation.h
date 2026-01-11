#pragma once
#include "esphome/core/defines.h"

#ifdef USE_SAFE_MODE_CALLBACK
#include "safe_mode.h"

#include "esphome/core/automation.h"

namespace esphome::safe_mode {

class SafeModeTrigger : public Trigger<> {
 public:
  explicit SafeModeTrigger(SafeModeComponent *parent) {
    parent->add_on_safe_mode_callback([this]() { trigger(); });
  }
};

}  // namespace esphome::safe_mode

#endif  // USE_SAFE_MODE_CALLBACK
