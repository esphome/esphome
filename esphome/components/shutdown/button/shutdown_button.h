#pragma once

#include "esphome/core/component.h"
#include "esphome/components/button/button.h"

namespace esphome::shutdown {

class ShutdownButton : public button::Button, public Component {
 public:
  void dump_config() override;

 protected:
  void press_action() override;
};

}  // namespace esphome::shutdown
