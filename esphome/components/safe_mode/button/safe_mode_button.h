#pragma once

#include "esphome/components/button/button.h"
#include "esphome/components/safe_mode/safe_mode.h"
#include "esphome/core/component.h"

namespace esphome::safe_mode {

class SafeModeButton final : public button::Button, public Component {
 public:
  void dump_config() override;
  void set_safe_mode(SafeModeComponent *safe_mode_component);

 protected:
  SafeModeComponent *safe_mode_component_;
  void press_action() override;
};

}  // namespace esphome::safe_mode
