#pragma once

#include "esphome/components/select/select.h"

namespace esphome {
namespace amg8833 {

class SetterSelect : public select::Select {
 public:
  using SetterCallback = std::function<void(const std::string &)>;
  SetterSelect() = default;
  void set_setter(SetterCallback setter) { this->setter_ = std::move(setter); }

 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    if (this->setter_)
      this->setter_(value);
  }
  SetterCallback setter_;
};

}  // namespace amg8833
}  // namespace esphome
