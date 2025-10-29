#pragma once

#include "esphome/components/button/button.h"
#include "esphome/components/safe_mode/safe_mode.h"
#include "esphome/core/component.h"

namespace esphome {
namespace safe_mode {

class SafeModeButton : public button::Button, public Component {
 public:
  void dump_config() override;
  void set_safe_mode(SafeModeComponent *safe_mode_component);

 protected:
  SafeModeComponent *safe_mode_component_;
  void press_action() override;
};

}  // namespace safe_mode
}  // namespace esphome
