#pragma once

#include "esphome/components/switch/switch.h"
#include "../dfrobot_c4001.h"

namespace esphome {
namespace dfrobot_c4001 {

class C4001Switch : public switch_::Switch, public Parented<C4001Component> {
 protected:
  void write_state(bool state) override;
};

}  // namespace dfrobot_c4001
}  // namespace esphome
