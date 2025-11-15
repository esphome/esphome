#pragma once
#include "safe_mode.h"

#include "esphome/core/automation.h"

namespace esphome {
namespace safe_mode {

class SafeModeTrigger : public Trigger<> {
 public:
  explicit SafeModeTrigger(SafeModeComponent *parent) {
    parent->add_on_safe_mode_callback([this]() { trigger(); });
  }
};

}  // namespace safe_mode
}  // namespace esphome
