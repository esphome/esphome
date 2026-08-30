#pragma once

#include "esphome/components/button/button.h"
#include "esphome/core/component.h"

namespace esphome::restart {

class RestartButton final : public button::Button, public Component {
 public:
  void dump_config() override;

 protected:
  void press_action() override;
};

}  // namespace esphome::restart
