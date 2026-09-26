#pragma once

#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"
#include "../hitachi_ac344.h"

namespace esphome::hitachi_ac344 {

class MildewProofSwitch final : public switch_::Switch, public Parented<HitachiClimate> {
 protected:
  void write_state(bool state) override;
};

}  // namespace esphome::hitachi_ac344
