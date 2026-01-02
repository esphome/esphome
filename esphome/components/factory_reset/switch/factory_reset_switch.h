#pragma once

#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"

namespace esphome {
namespace factory_reset {

class FactoryResetSwitch : public switch_::Switch, public Component {
 public:
  void dump_config() override;
#ifdef USE_OPENTHREAD
  static void factory_reset_callback();
#endif

 protected:
  void write_state(bool state) override;
};

}  // namespace factory_reset
}  // namespace esphome
