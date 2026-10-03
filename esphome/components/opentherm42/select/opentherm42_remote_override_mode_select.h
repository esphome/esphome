#pragma once

#include "esphome/core/component.h"
#include "esphome/components/select/select.h"

namespace esphome::opentherm42 {

// §5.3.8.3 Class 8, ID 99: Operating Mode HC1/HC2/DHW nibble, shared by all three. Unlike
// OpenTherm42Select (write-only, no readback), this one is read back every informational rotation
// (see hub.cpp's REMOTE_OVERRIDE_OPERATING_MODES_READ case), so there's no initial_option or flash
// persistence -- it stays Unknown until that first real read, the safe "No Override" default is what
// build_next_request_() sends in the meantime (see its REMOTE_OVERRIDE_OPERATING_MODES case).
class OpenTherm42RemoteOverrideModeSelect : public select::Select, public Component {
 protected:
  void control(size_t index) override { this->publish_state(index); }
  void dump_config() override;
};

}  // namespace esphome::opentherm42
