#pragma once

#include "esphome/components/switch/switch.h"
#include "../ld2450.h"

namespace esphome {
namespace ld2450 {

class MultiTargetSwitch : public switch_::Switch, public Parented<LD2450Component> {
 public:
  MultiTargetSwitch() = default;

 protected:
  void write_state(bool state) override;
};

}  // namespace ld2450
}  // namespace esphome
