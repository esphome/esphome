#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"

namespace esphome {
namespace restart {

class RestartSwitch : public switch_::Switch, public Component {
 public:
  explicit RestartSwitch(const std::string &name) : switch_::Switch(name) {}

  void dump_config() override;

 protected:
  void write_state(bool state) override;
};

}  // namespace restart
}  // namespace esphome
