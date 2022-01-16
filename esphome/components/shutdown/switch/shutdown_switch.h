#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"

namespace esphome {
namespace shutdown {

class ShutdownSwitch : public switch_::Switch, public Component {
 public:
  void dump_config() override;

 protected:
  void write_state(bool state) override;
};

}  // namespace shutdown
}  // namespace esphome
