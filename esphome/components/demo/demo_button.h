#pragma once

#include "esphome/components/button/button.h"
#include "esphome/core/log.h"

namespace esphome::demo {

class DemoButton : public button::Button {
 protected:
  void press_action() override {}
};

}  // namespace esphome::demo
