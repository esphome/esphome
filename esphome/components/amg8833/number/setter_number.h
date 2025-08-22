#pragma once

#include "esphome/components/number/number.h"

namespace esphome {
namespace amg8833 {

class SetterNumber : public number::Number {
 public:
  using SetterCallback = std::function<void(float)>;
  SetterNumber() = default;
  void set_setter(SetterCallback setter) { this->setter_ = std::move(setter); }

 protected:
  void control(float value) override {
    this->publish_state(value);
    if (this->setter_)
      this->setter_(value);
  }
  SetterCallback setter_;
};

}  // namespace amg8833
}  // namespace esphome
