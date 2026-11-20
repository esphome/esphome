#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"

namespace esphome::opentherm42 {

// §5.3.8.3 Class 8, ID 99 HB bit 4: Manual DHW push2. Unlike OpenTherm42Switch's write-only ids (no
// way to independently verify against the boiler), this bit is read back every informational
// rotation (see hub.cpp's REMOTE_OVERRIDE_OPERATING_MODES_READ case), so there's no restore_mode --
// it stays at the safe "no push" default (see build_next_request_()'s REMOTE_OVERRIDE_OPERATING_MODES
// case) until that first real read arrives.
class OpenTherm42ManualDhwPush2Switch : public switch_::Switch, public Component {
 protected:
  void write_state(bool state) override { this->publish_state(state); }
  void dump_config() override;
};

}  // namespace esphome::opentherm42
