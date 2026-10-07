#pragma once

#include "esphome/components/switch/switch.h"

#include "tormatic_cover.h"

namespace esphome::tormatic {

class TormaticSwitch : public switch_::Switch {
 public:
  explicit TormaticSwitch(Tormatic *parent) : parent_(parent) {}

 protected:
  void write_state(bool state) override {
    this->parent_->send_light_command(state);
    this->publish_state(state);
  }

  Tormatic *parent_;
};

}  // namespace esphome::tormatic
