#pragma once

#include "../dfrobot_c4004.h"
#include "esphome/components/select/select.h"

namespace esphome {
namespace dfrobot_c4004 {

class C4004InstallModeSelect : public Component, public select::Select, public Parented<C4004Component> {
 protected:
  void control(const std::string &value) override;
};

}  // namespace dfrobot_c4004
}  // namespace esphome
