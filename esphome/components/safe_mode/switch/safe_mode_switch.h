#pragma once

#include "esphome/components/safe_mode/safe_mode.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"

namespace esphome {
namespace safe_mode {

class SafeModeSwitch : public switch_::Switch, public Component {
 public:
  void dump_config() override;
  void set_safe_mode(SafeModeComponent *safe_mode_component);

 protected:
  SafeModeComponent *safe_mode_component_;
  void write_state(bool state) override;
};

}  // namespace safe_mode
}  // namespace esphome
