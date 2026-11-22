#pragma once

#include "esphome/components/switch/switch.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"

namespace esphome::lvgl {

class LVGLSwitch final : public switch_::Switch, public Component {
 public:
  explicit LVGLSwitch(void (*state_lambda)(bool)) : state_lambda_(state_lambda) {}

  void setup() override { this->restore(false); }

 protected:
  void write_state(bool value) override { this->state_lambda_(value); }
  void (*state_lambda_)(bool);
};

}  // namespace esphome::lvgl
