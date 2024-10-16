#pragma once

#include "esphome/components/switch/switch.h"
#include "../si4713.h"

namespace esphome {
namespace si4713 {

class AsqIalhEnableSwitch : public switch_::Switch, public Parented<Si4713Component> {
 public:
  AsqIalhEnableSwitch() = default;

 protected:
  void write_state(bool value) override;
};

}  // namespace si4713
}  // namespace esphome