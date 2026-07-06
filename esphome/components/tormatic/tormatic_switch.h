#pragma once

#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"

#include "tormatic_cover.h"

namespace esphome::tormatic {

class TormaticSwitch : public switch_::Switch, public Component {
 public:
  void set_parent(Tormatic *parent) { this->parent_ = parent; }

  void setup() override {
    this->parent_->add_on_light_state_callback([this](bool state) { this->publish_state(state); });
  }

  void dump_config() override;

 protected:
  void write_state(bool state) override {
    this->parent_->send_light_command(state);
    this->publish_state(state);
  }

  Tormatic *parent_{nullptr};
};

}  // namespace esphome::tormatic
