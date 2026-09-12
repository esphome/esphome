#pragma once

#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"

namespace esphome::factory_reset {

class FactoryResetSwitch final : public switch_::Switch, public Component {
 public:
  // User provided, not "= default": `new(p) FactoryResetSwitch()` would zero-fill .bss that is already zero.
  FactoryResetSwitch() {}

  void dump_config() override;
#ifdef USE_OPENTHREAD
  static void factory_reset_callback();
#endif

 protected:
  void write_state(bool state) override;
};

}  // namespace esphome::factory_reset
