#pragma once

#include "esphome/components/button/button.h"
#include "esphome/core/component.h"

namespace esphome {
namespace shutdown {

class ShutdownButton : public button::Button, public Component {
 public:
  void dump_config() override;

 protected:
  void press_action() override;
};

}  // namespace shutdown
}  // namespace esphome
