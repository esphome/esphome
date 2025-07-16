#pragma once

#include "esphome/core/defines.h"

#include "esphome/components/button/button.h"
#include "esphome/core/component.h"

namespace esphome {
namespace factory_reset {

class FactoryResetButton : public button::Button, public Component {
 public:
  void dump_config() override;
#ifdef USE_OPENTHREAD
  static void factory_reset_callback();
#endif

 protected:
  void press_action() override;
};

}  // namespace factory_reset
}  // namespace esphome
