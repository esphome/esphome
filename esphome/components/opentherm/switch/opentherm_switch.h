#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/log.h"

namespace esphome::opentherm {

class OpenthermSwitch : public switch_::Switch, public Component {
 protected:
  void write_state(bool state) override;

 public:
  void setup() override;
  void dump_config() override;
};

}  // namespace esphome::opentherm
