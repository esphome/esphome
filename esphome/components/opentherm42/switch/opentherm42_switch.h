#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"

namespace esphome::opentherm42 {

// Thin switch shim: the boiler write is inherently "fire and forget" (packed into the next §4.3
// conversation the hub sends), so there's no hardware to fail against -- accept the commanded state
// immediately.
class OpenTherm42Switch : public switch_::Switch, public Component {
 protected:
  void write_state(bool state) override;

 public:
  void setup() override;
  void dump_config() override;
};

}  // namespace esphome::opentherm42
