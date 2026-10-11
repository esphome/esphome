#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "text.h"

namespace esphome::text {

class TextStateTrigger final : public Trigger<std::string> {
 public:
  explicit TextStateTrigger(Text *parent) {
    parent->add_on_state_callback([this](const std::string &value) { this->trigger(value); });
  }
};

}  // namespace esphome::text
