#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"

namespace esphome::copy {

class CopySwitch final : public switch_::Switch, public Component {
  SUB_SWITCH(source)

 public:
  void setup() override;
  void dump_config() override;

 protected:
  void write_state(bool state) override;
};

}  // namespace esphome::copy
