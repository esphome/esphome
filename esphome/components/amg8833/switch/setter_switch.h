#pragma once

#include "esphome/components/switch/switch.h"

namespace esphome {
namespace amg8833 {

class SetterSwitch : public switch_::Switch {
 public:
  using SetterCallback = std::function<void(bool)>;
  SetterSwitch() = default;
  void set_setter(SetterCallback setter) { this->setter_ = std::move(setter); }

 protected:
  void write_state(bool state) override {
    this->publish_state(state);
    if (this->setter_)
      this->setter_(state);
  }
  SetterCallback setter_;
};

}  // namespace amg8833
}  // namespace esphome
