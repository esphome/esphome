#pragma once

#include "esphome/components/switch/switch.h"
#include "esphome/core/helpers.h"

#include "tormatic_cover.h"

namespace esphome::tormatic {

class TormaticLightSwitch final : public switch_::Switch, public Parented<Tormatic> {
 protected:
  void write_state(bool state) override;
};

}  // namespace esphome::tormatic
