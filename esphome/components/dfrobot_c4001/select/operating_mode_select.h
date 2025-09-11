#pragma once

#include "../dfrobot_c4001.h"
#include "esphome/components/select/select.h"

namespace esphome {
namespace dfrobot_c4001 {

class C4001Select : public Component, public select::Select, public Parented<c4001Component> {
 public:
  C4001Select() = default;

 protected:
  void control(const std::string &value) override;
};

}  // namespace dfrobot_c4001
}  // namespace esphome
