#pragma once

#include "esphome/core/defines.h"

#ifdef USE_SWITCH

#include "esphome/components/switch/switch.h"
#include "ld600x.h"

namespace esphome::ld600x {

class LD600XSwitch final : public switch_::Switch, public Parented<LD600XComponent> {
 public:
  explicit LD600XSwitch(uint8_t kind) : kind_(kind) {}

 protected:
  void write_state(bool state) override;

  uint8_t kind_;
};

}  // namespace esphome::ld600x

#endif  // USE_SWITCH
