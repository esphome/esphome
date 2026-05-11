#pragma once

#ifdef USE_ESP32
#include "esphome/components/fendt_caravan/caravan_component_base.h"
#include "esphome/components/fendt_caravan/variable.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/string_ref.h"
#include "esphome/core/log.h"

namespace esphome::fendt_caravan {

class FendtSwitch : public CaravanComponentBase<bool>, public switch_::Switch {
 public:
  void setup() override {
    if (this->key_name_.empty())
      return;
    auto *variable = static_cast<Variable<bool> *>(this->get_parent()->get_variable(this->key_name_));
    if (variable != nullptr) {
      this->set_variable(variable);
    }
  }

 protected:
  void write_state(bool state) override {
    std::string command;
    if (this->variable_) {
      this->variable_->set_value(state);
      command = this->variable_->get_command();
    }
    this->parent_->on_state_change_command(this->key_name_, command);
  }
  void on_decoded(const bool &value) override { this->publish_state(value); }

 private:
};

}  // namespace esphome::fendt_caravan
#endif
