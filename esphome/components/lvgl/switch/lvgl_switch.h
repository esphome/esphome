#pragma once

#include <utility>

#include "esphome/components/switch/switch.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace lvgl {

class LVGLSwitch : public switch_::Switch, public Component {
 public:
  LVGLSwitch(std::function<void(bool)> state_lambda) : state_lambda_(std::move(state_lambda)) {}

  void setup() override { this->write_state(this->get_initial_state_with_restore_mode().value_or(false)); }

 protected:
  void write_state(bool value) override { this->state_lambda_(value); }
  std::function<void(bool)> state_lambda_{};
};

}  // namespace lvgl
}  // namespace esphome
