#pragma once

#include "esphome/core/component.h"
#include "esphome/components/select/select.h"
#include "../hub.h"

namespace esphome::opentherm42 {

// §5.3.8.3 Class 8, ID 99: Operating Mode HC1/HC2/DHW nibble, shared by all three. Unlike
// OpenTherm42Select (write-only, no readback), this one is read back every informational rotation
// (see hub.cpp's REMOTE_OVERRIDE_OPERATING_MODES_READ case), so there's no initial_option or flash
// persistence -- it stays Unknown until that first real read, the safe "No Override" default is what
// build_next_request_() sends in the meantime (see its REMOTE_OVERRIDE_OPERATING_MODES case).
// control() also pushes the commanded index to the hub, which builds the id=99 write from it instead of
// from this entity's ->active_index() (nullopt once invalidated) -- see hub.h's
// set_remote_override_mode_write_value(). hub and mode are required and never change after construction.
class OpenTherm42RemoteOverrideModeSelect : public select::Select, public Component {
 public:
  OpenTherm42RemoteOverrideModeSelect(OpenTherm42Hub *hub, RemoteOverrideMode mode) : hub_(hub), mode_(mode) {}

 protected:
  void control(size_t index) override;
  void dump_config() override;

  OpenTherm42Hub *hub_;
  RemoteOverrideMode mode_;
};

}  // namespace esphome::opentherm42
