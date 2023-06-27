#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"

namespace esphome {
namespace factory_reset {

class FactoryResetSwitch : public switch_::Switch, public Component {
 public:
  void dump_config() override;

 protected:
  void write_state(bool state) override;
};

}  // namespace factory_reset
}  // namespace esphome
